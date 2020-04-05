#include "QueryThread.h"
#include <vector>
#include "CardData.h"
#include "CardDatabase.h"
#include <mutex>
#include <opencv2/photo/photo.hpp>
#include <opencv2/imgproc/types_c.h>
#include <iterator>

#ifdef _WIN32
#include <windows.h>
#define SLEEP(MS) Sleep((MS))
#else
#include <unistd.h>
#define SLEEP(MS) usleep((MS) * 1000)
#endif

extern const char* appData;
extern std::mutex g_resultsMutex;

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

enum OPERATOR
{
	MORPH_OPENING = 2,
	MORPH_CLOSING = 3
};

// #pragma optimize("",off)

// http://stackoverflow.com/a/30418912/5008845
cv::Rect findMinRect(const cv::Mat1b& src)
{
	cv::Mat1f W(src.rows, src.cols, float(0));
	cv::Mat1f H(src.rows, src.cols, float(0));

	cv::Rect maxRect(0, 0, 0, 0);
	float maxArea = 0.f;

	for (int r = 0; r < src.rows; ++r)
	{
		for (int c = 0; c < src.cols; ++c)
		{
			if (src(r, c) == 0)
			{
				H(r, c) = 1.f + ((r>0) ? H(r - 1, c) : 0);
				W(r, c) = 1.f + ((c>0) ? W(r, c - 1) : 0);
			}

			float minw = W(r, c);
			for (int h = 0; h < H(r, c); ++h)
			{
				minw = min(minw, W(r - h, c));
				float area = (h + 1) * minw;
				if (area > maxArea)
				{
					maxArea = area;
					maxRect = cv::Rect(cv::Point(c - (int)minw + 1, r - h), cv::Point(c + 1, r + 1));
				}
			}
		}
	}

	return maxRect;
}


cv::RotatedRect largestRectInNonConvexPoly(std::vector<cv::Point> ptz)//const cv::Mat1b& src)
{
	// Create a matrix big enough to not lose points during rotation
// 	std::vector<cv::Point> ptz;
// 	cv::findNonZero(src, ptz);
	cv::Rect bbox = cv::boundingRect(ptz);//224,215
	for (cv::Point& pt : ptz)
	{
		pt.x -= bbox.x;
		pt.y -= bbox.y;
	}

	int maxdim = max(bbox.width, bbox.height);
	cv::Mat1b work(2 * maxdim, 2 * maxdim, uchar(0));
	cv::fillConvexPoly(work(cv::Rect(maxdim - bbox.width / 2, maxdim - bbox.height / 2, bbox.width, bbox.height)), ptz, 255);

	// Store best data
	cv::Rect bestRect;
	int bestAngle = 0;

	// For each angle
	for (int angle = 0; angle < 90; angle += 1)
	{
// 		cout << angle << endl;

		// Rotate the image
		cv::Mat R = getRotationMatrix2D(cv::Point(maxdim, maxdim), angle, 1);
		cv::Mat1b rotated;
		cv::warpAffine(work, rotated, R, work.size());

		// Keep the crop with the polygon
		std::vector<cv::Point> pts;
		cv::findNonZero(rotated, pts);
		cv::Rect box = cv::boundingRect(pts);
		cv::Mat1b crop = rotated(box).clone();

		// Invert colors
		crop = ~crop;

		// Solve the problem: "Find largest rectangle containing only zeros in an binary matrix"
		// http://stackoverflow.com/questions/2478447/find-largest-rectangle-containing-only-zeros-in-an-n%C3%97n-binary-matrix
		cv::Rect r = findMinRect(crop);

		// If best, save result
		if (r.area() > bestRect.area())
		{
			bestRect = r + box.tl();    // Correct the crop displacement
			bestAngle = angle;
		}
	}

	// Apply the inverse rotation
	cv::Mat Rinv = cv::getRotationMatrix2D(cv::Point(maxdim, maxdim), -bestAngle, 1);
	std::vector<cv::Point> rectPoints{ bestRect.tl(), cv::Point(bestRect.x + bestRect.width, bestRect.y), bestRect.br(), cv::Point(bestRect.x, bestRect.y + bestRect.height) };
	std::vector<cv::Point> rotatedRectPoints;
	cv::transform(rectPoints, rotatedRectPoints, Rinv);

	// Apply the reverse translations
	for (int i = 0; i < rotatedRectPoints.size(); ++i)
	{
		rotatedRectPoints[i] += bbox.tl() - cv::Point(maxdim - bbox.width / 2, maxdim - bbox.height / 2);
	}

	// Get the rotated rect
	cv::RotatedRect rrect = cv::minAreaRect(rotatedRectPoints);

	return rrect;
}

static cv::Mat debugArea;
const int VIDEOWIDTH = 1280;
const int VIDEOHEIGHT = 720;

Query::Query(CardDatabase& aCardDatabase)
	:myCardDatabase(aCardDatabase)
, myToplistSize(1)
, myMaxRotation(-1.f)
, myLastOkMatch(0)
, myAutoMatchTimeout(3000)
, myGoodMatchScore(170)
, myOkMatchScore(270)
, myAlreadyMatchedMaxSize(12)
, myIsAutoMatch(false)
, myDecodedScreenBuffer(VIDEOWIDTH, VIDEOHEIGHT, CV_8UC4)
, myScreenScale(1.0f)
, myMinCardHeightRelative(0.05f)
, myMaxCardHeightRelative(0.2f)
{

}

std::string Query::TestBuffer(unsigned char* aBuffer, int aWidth, int aHeight)
{
	cv::Mat primeCase = cv::Mat(aWidth, aHeight, CV_8UC4, (void*)aBuffer);
	cv::Mat bgr;
	cv::cvtColor(primeCase, bgr, cv::COLOR_RGBA2BGR);

	UpdateSearchSettings();

	Result result;
	if (FindCardInRoi(mySettings, myCardDatabase.myCardLists, bgr, true, result))
	{
		return result.myMatch.myList[0].myDatabaseCard->myCardName;
	}
	return "fail";
}

bool Query::TestFile(const std::string &file, bool aMatchingNameOnly/*=false*/)
{
	const int start = (int)file.find_last_of("/") + 1;
	int end = (int)file.find_last_of(".");
	int parenthesis = (int)file.find_last_of("("); //multiple downloads does this
	if (parenthesis != -1)
	{
		end = parenthesis - 1;
	}

	const std::string name = file.substr(start, end - start);

	std::cout << name << " ";

	cv::Mat primeCase = cv::imread(file);
	if (primeCase.cols == 0 || primeCase.rows == 0)
		return false;

	Result result;
// 	myCardDatabase.myCurrentFormat = CardDatabase::STANDARD;

	//   				query.testname = "Street Wraith";
	UpdateSearchSettings();

	if (!aMatchingNameOnly)
	{
		const int before = TimeNow();
		FindCardInRoiAndPrint(primeCase, myCardDatabase.myCardLists, result);
		std::cout << (TimeNow() - before) << "ms ";
	}
	std::cout << std::endl;

	Result extraResult;
	static bool testIndividual = true;
	if (testIndividual)
	{
		int savedOkMatchScore = myOkMatchScore;
		myOkMatchScore = 400;
		testname = name;
		CardList set;
		myCardDatabase.GetCardsByName(testname, set);
		std::vector<const CardList*> sets;
		sets.push_back(&set);

		FindCardInRoiAndPrint(primeCase, sets, extraResult);
		myOkMatchScore = savedOkMatchScore;
	}

	int fIndex = -1;
	for (int i = 0, e = (int)result.myMatch.myList.size(); i < e; ++i)
	{
		if (CardDatabase::GetHashname(result.myMatch.myList[i].myDatabaseCard->myCardName) == CardDatabase::GetHashname(name))
		{
			fIndex = i;
			break;
		}
	}

	if (fIndex == 0)
	{
		std::cout << "[Success] Score: " << result.myMatch.myList[fIndex].myScore[0] << " Quick:" << result.myMatch.myList[fIndex].myScore[1] << " Path:" << result.myMatch.myList[fIndex].myInput.myDebugPath;
	}
	else
	{
		std::cout << "[Failed ] ";
		if (extraResult.myMatch.myList.size())
		{
			std::cout << "Extra Score:" << extraResult.myMatch.myList[0].myScore[0] << " Quick:" << extraResult.myMatch.myList[0].myScore[1] << " Path:" << extraResult.myMatch.myList[0].myInput.myDebugPath;
		}
		if (result.myMatch.myList.size())
		{
			std::cout << "Best:" << result.myMatch.myList[0].myScore[0] << " Quick:" << result.myMatch.myList[0].myScore[1] << " " << result.myMatch.myList[0].myDatabaseCard->myCardName;
		}
	}
	std::cout << std::endl << std::endl;
	return fIndex >= 0 && fIndex < 4;
}

int Query::SetSetting(const std::string& key, const std::string& value)
{
	if (strcmp(key.c_str(), "mincardsize") == 0)
	{
		int minSize = atoi(value.c_str());
		if (minSize >= 0 && minSize <= 100)
		{
			myMinCardHeightRelative = float(minSize)/100.f;
			return (int)(100 * myMinCardHeightRelative);
		}
	}
	else if (strcmp(key.c_str(), "maxcardsize") == 0)
	{
		int maxSize = atoi(value.c_str());
		if (maxSize >= 0 && maxSize <= 100)
		{
			myMaxCardHeightRelative = float(maxSize) / 100.f;
			return (int)(100 * myMaxCardHeightRelative);
		}
	}
	else if (strcmp(key.c_str(), "automatchhistorysize") == 0)
	{
		int size = atoi(value.c_str());
		if (size > 0)
		{
			myAlreadyMatchedMaxSize = size;
			return myAlreadyMatchedMaxSize;
		}
	}
	else if (strcmp(key.c_str(), "okscore") == 0)
	{
		int okscore = atoi(value.c_str());
		if (okscore >= 0 && okscore <= 100)
		{
			myOkMatchScore = (1024 * (100 - okscore)) / 100;
			return myOkMatchScore;
		}
	}
	else if (strcmp(key.c_str(), "goodscore") == 0)
	{
		int goodscore = atoi(value.c_str());
		if (goodscore >= 0 && goodscore <= 100)
		{
			myGoodMatchScore = (1024 * (100 - goodscore)) / 100;
			return myGoodMatchScore;
		}
	}
	return -1;
}

inline bool Deviates(cv::Vec3b value, int someValue)
{
	int thresh = 5;
	for (int i = 0; i<3; ++i)
	{
		if (std::abs(value(i) - someValue) > thresh)
		{
			return true;
		}
	}
	return false;
}
inline bool Deviates(cv::Vec3b value, cv::Vec3b anotherValue)
{
	int thresh = 5;
	for (int i = 0; i<3; ++i)
	{
		if (std::abs(value(i) - anotherValue(i)) > thresh)
		{
			return true;
		}
	}
	return false;
}
inline bool IsColor(cv::Vec3b value)
{
	int avgValue = (value(0) + value(1) + value(2)) / 3;
	if (avgValue > 75 || avgValue < 175)
	{
		return true;
	}
	return Deviates(value, avgValue);
}

inline bool IsDark(cv::Vec3b value)
{
	const int dark = 90;
	int r = value(0);
	int g = value(1);
	int b = value(2);
	return (r < dark) && (g < dark) && (b < dark);
}

inline bool IsDark(const cv::Mat& aMat, int aStartX, int anEndX, int aStartY, int anEndY)
{
	int totalPixels = ((anEndX - aStartX) + 1) * ((anEndY - aStartY) + 1);
	int darkness = 0;
	for (int x = aStartX; x <= anEndX; ++x)
	{
		for (int y = aStartY; y <= anEndY; ++y)
		{
			cv::Vec3b val = aMat.at<cv::Vec3b>(y, x);
			if (IsDark(val) && !IsColor(val))
			{
				++darkness;
			}
		}
	}

	return darkness * 10 > totalPixels * 9;
}

inline bool HasColor(const cv::Mat& aMat, int aStartX, int anEndX, int aStartY, int anEndY)
{
	int totalPixels = ((anEndX - aStartX) + 1) * ((anEndY - aStartY) + 1);
	int color = 0;
	cv::Vec3b avg = aMat.at<cv::Vec3b>(aStartY, aStartX);

	for (int x = aStartX; x <= anEndX; ++x)
	{
		for (int y = aStartY; y <= anEndY; ++y)
		{
			cv::Vec3b current = aMat.at<cv::Vec3b>(y, x);
			if (IsColor(current))
			{
				return true;
			}

			// 			if (Deviates(current, avg))
			// 			{
			// 				return true;
			// 			}

			avg += current;
		}
	}
	return false;
}

cv::Mat getRotatedRectImage(const cv::RotatedRect& aRect, const cv::Mat& anImage)
{
	// get angle and size from the bounding box
	float angle = aRect.angle;
	cv::Size rect_size((int)std::ceil(aRect.size.width), (int)std::ceil(aRect.size.height));

// 	const bool useRect = std::abs(fmod(angle, 90.f)) < 0.1f;
// 	if (useRect)
// 	{
// 		if (angle == 0.f)
// 		{
// 			return cv::Mat(anImage, cv::Rect(aRect.center.x - rect_size.width / 2, aRect.center.y - rect_size.height / 2, rect_size.width, rect_size.height));
// 		}
// 	}

// 	// thanks to http://felix.abecassis.me/2011/10/opencv-rotation-deskewing/
// 	if (aRect.angle < -45.) {
// 		angle += 90.0;
// 		std::swap(rect_size.width, rect_size.height);
// 	}
	/************************************************************************/
	/* EXTEND SIZE FROM TOP TO BOTTOM TO MATCH EXPECTED                     */
	/************************************************************************/
	// get the rotation matrix
	cv::Mat rotation = cv::getRotationMatrix2D(aRect.center, angle, 1.0);
	cv::Mat rotated(rect_size, anImage.type(), cv::Scalar(122,122,122,255));

	double wdiff = (double)((aRect.center.x - ((double)(rotated.cols) / 2.0)));
	double hdiff = (double)((aRect.center.y - ((double)(rotated.rows) / 2.0)));
	rotation.at<double>(0, 2) -= wdiff; //Adjust the rotation point to the middle of the dst image
	rotation.at<double>(1, 2) -= hdiff;

	// perform the affine transformation
	cv::warpAffine(anImage, rotated, rotation, rotated.size(), cv::INTER_LANCZOS4);
#ifdef DEBUGIMAGES
	cv::Mat debugRect = rotated;
#endif

	return rotated;
}

float Width(const cv::RotatedRect& rect)
{
	return min(rect.size.width, rect.size.height);
}
float Height(const cv::RotatedRect& rect)
{
	return max(rect.size.width, rect.size.height);
}
float Length2(const cv::Point2f& aPoint)
{
	return aPoint.x*aPoint.x + aPoint.y*aPoint.y;
}

bool PointInRect(const cv::RotatedRect& rect, int poiX, int poiY)
{
	std::vector<cv::Point2f> cvtd;
	cvtd.resize(4);
	rect.points(&cvtd[0]);

	double dist = cv::pointPolygonTest(cvtd, cv::Point(poiX, poiY), true);
	return dist >= 0.f;
}

bool PointInRect(const PotentialRect& rect, int poiX, int poiY)
{
	return PointInRect(rect.myRotatedRect, poiX, poiY);
}

float GetRectRatio(const cv::RotatedRect& rect)
{
	return Width(rect) / Height(rect);
}

float PerfectRatioDiff(float aValue)
{
	const float perfect(63.f / 88.f);
	return std::abs((aValue / perfect) - 1.f);
}

float PerfectRatioDiff(const cv::RotatedRect& rect)
{
	return PerfectRatioDiff(GetRectRatio(rect));
}

bool IsValidCardRect(const cv::RotatedRect& rect, const float maxCardHeight)
{
	const float minCardHeight = 48.f;//hard to do anything at that size
	const float width = Width(rect);
	const float height = Height(rect);
	if (height > maxCardHeight)
	{
		return false;
	}

	if (width < minCardHeight*0.74f)
	{
		return false;
	}

	const float fullCardRatio = width / height;
	const float ratioDiff = PerfectRatioDiff(fullCardRatio);
	if (ratioDiff > 0.03f)
	{
		return false;
	}

	return true;
}

bool IsValidImageRatio(int cols, int rows)
{
	const float fullCardRatio = ((float)cols) / ((float)rows);
	const float ratioDiff = PerfectRatioDiff(fullCardRatio);
	if (ratioDiff > 0.03f)
	{
		return false;
	}
	return true;
}

float SideSum(const cv::RotatedRect& rect) { return rect.size.width + rect.size.height; }

float SizeScale(const cv::RotatedRect& lhs, float aSize)
{
	float lhsSum = SideSum(lhs);
	float scale = lhsSum > aSize ? aSize / lhsSum : lhsSum / aSize;
	return scale;
}

bool SizeMatch(const cv::RotatedRect& lhs, float aSize)
{
	return SizeScale(lhs, aSize) > 0.93f;
}

bool SizeMatch(const cv::RotatedRect& lhs, const cv::RotatedRect& rhs)
{
	return SizeMatch(lhs, SideSum(rhs));
}

int UpdateTopList(std::vector<Match>& ioList, const Match& iNew, const int topListSize, const float okMatchScore)
{
	int lowerThan = -1;
	int foundIndex = -1;
	for (int i = 0, e = (int)ioList.size(); i < e; ++i)
	{
		Match& match = ioList[i];
		if (match.myDatabaseCard == iNew.myDatabaseCard || match.myDatabaseCard->myCardName.compare(iNew.myDatabaseCard->myCardName)==0)
		{
			foundIndex = i;
		}
		if (lowerThan == -1 && iNew.myScore[0] < match.myScore[0])
		{
			lowerThan = i;
		}
	}

	if (lowerThan!=-1)
	{
		if (lowerThan <= foundIndex)
		{
			ioList.erase(ioList.begin() + foundIndex);
			foundIndex = -1;
		}

		if (foundIndex == -1)
		{
			ioList.insert(ioList.begin() + lowerThan, iNew);
		}
	}
	else if (ioList.size() < topListSize && foundIndex == -1)
	{
		ioList.push_back(iNew);
	}

	if (ioList.size() > topListSize)
	{
		ioList.pop_back();
	}

	if (ioList.size() < topListSize)
	{
		return okMatchScore;
	}
	return ioList[ioList.size()-1].myScore[0];
}

bool Query::FindBestMatch(std::vector<PotentialCardMatches>& underMouseCards, const std::vector<const CardList*>& iCardSets, const SearchSettings& inputs, const cv::Mat& aroundCardArea, Result& oResult)
{
	if (underMouseCards.size() == 0)
	{
		return false;
	}

	int worst = myOkMatchScore;
	const int earlyOutScore = 200;

	int listSize = (int)underMouseCards.size();
	for (int inputIndex = 0; inputIndex < listSize; ++inputIndex)
	{
		for (int rectIndex = 0, e= underMouseCards[inputIndex].myCard.myPotenatialRects.size(); rectIndex < e; ++rectIndex)
		{
			PotentialRect potentialRect = underMouseCards[inputIndex].myCard.myPotenatialRects[rectIndex];
			if (!potentialRect.myVariations.size())
			{
				potentialRect.CreateBaseImage(aroundCardArea);
			}

			std::vector<Match>& matchList = underMouseCards[inputIndex].myList;
			worst = FindPotentialRectMatches(potentialRect, iCardSets, worst, matchList);
			for (Match& m : matchList)
			{
				m.myPotentialRectIndex = rectIndex;
			}

			if (worst < earlyOutScore && worst < myGoodMatchScore)
				break;
		}
	}

	for (PotentialCardMatches& pcm : underMouseCards)
	{
		if (pcm.myList.size())
			pcm.myScore = pcm.myList[0].myScore[0];
	}

	auto scoreSort = [](const PotentialCardMatches& lhs, const PotentialCardMatches& rhs)
	{
		return lhs.myScore < rhs.myScore;
	};
	std::sort(underMouseCards.begin(), underMouseCards.end(), scoreSort);
	
	const PotentialCardMatches& bestMatch = underMouseCards[0];
	if (bestMatch.myList.size())
	{
		if (!oResult.myMatch.myList.size() || bestMatch.myScore < oResult.myMatch.myScore)
		{
			oResult.myMatch = bestMatch;
		}
	}

	if (oResult.myMatch.myList.size())
	{
		oResult.myPoint = myPoint;
		myLastResult = oResult;
		return true;
	}
	return false;
}

int Query::FindPotentialRectMatches(PotentialRect& aPotentialRect, const std::vector<const CardList*>& iCardSets, int aWorst, std::vector<Match>& oMatchList)
{
	const int quickCap = myOkMatchScore / 18;//14; //16 gives +30% at 
	const bool useEarlyOut = myGoodMatchScore < 200;

	int bestHamming = aWorst;
	int bestQuick = -1;
	const CardData* bestCard = nullptr;
	int bestVariation = -1;

	for (int x = 0, xe = (int)aPotentialRect.myVariations.size(); x < xe; ++x)
	{
		CardInput& input = aPotentialRect.myVariations[x];
		CardData& queryCard = input.myQuery;
#ifdef DEBUGIMAGES
		cv::Mat debugMatch = queryCard.myInputImage;
#endif
		const ImageHash queryHash = queryCard.GetHash();

		for (const CardList* cardSet : iCardSets)
		{
			for (int i = 0, e = (int)cardSet->myCardData.size(); i < e; ++i)
			{
				const ImageHash& dbHash = cardSet->myHashes[i];
				int quickHamming = dbHash.QuickHammingDistance(queryHash);
				if (quickHamming > quickCap)
				{
					continue;
				}

				int hamming = dbHash.HammingDistance(queryHash);

				if (hamming < bestHamming)
				{
					bestQuick = quickHamming;
					bestHamming = hamming;
					bestCard = cardSet->myCardData[i];
					bestVariation = x;
				}

				if (useEarlyOut && bestHamming < myGoodMatchScore)
					break;
			}
			if (useEarlyOut && bestHamming < myGoodMatchScore)
				break;
		}

		if (useEarlyOut && bestHamming < myGoodMatchScore)
			break;
	}

	if (bestCard != nullptr)
	{
#ifdef DEBUGIMAGES
		cv::Mat debugInput = aPotentialRect.myVariations[bestVariation].myQuery.myInputImage;
		cv::Mat debugMatch = bestCard->myInputImage;
#endif
		Match currentMatch(bestCard);
		currentMatch.myScore[0] = bestHamming;
		currentMatch.myScore[1] = bestQuick;
		currentMatch.myVariant = bestVariation;
		currentMatch.myInput = aPotentialRect.myVariations[bestVariation];
		aWorst = UpdateTopList(oMatchList, currentMatch, myToplistSize, myOkMatchScore);
	}
	return aWorst;
}

bool Query::FindCardInRoiAndPrint(uint8_t *aBuffer, int aBufferLength, int aWidth, int aHeight, Result &r)
{
	static bool usePng = false;
	cv::Mat bgr;
	if (usePng)
	{
		cv::Mat rawData = cv::Mat(1, aBufferLength, CV_8UC1, aBuffer);
		bgr = cv::imdecode(rawData, cv::IMREAD_UNCHANGED);
	}
	else
	{
		bgr = cv::Mat(aWidth, aHeight, CV_8UC4, aBuffer);
	}

	if (bgr.empty())
	{
		printf("Failed to decode buffer of length: %i\n", aBufferLength);
		return false;
	}
	return FindCardInRoiAndPrint(bgr, myCardDatabase.myCardLists, r);
}

bool Query::FindCardInRoiAndPrint(const cv::Mat& source, const std::vector<const CardList*>& iCardSets, Result& r)
{
	UpdateSearchSettings();
	cv::Mat resized;
	cv::resize(source, resized, cv::Size(source.size().width * myScreenScale, source.size().height * myScreenScale), cv::INTER_LANCZOS4);

	if (FindCardInRoi(mySettings, iCardSets, resized, true, r))
	{
		return true;
	}
	return false;

}

bool Query::AddScreenAndPrint(uint8_t *aBuffer, int aBufferLength, int aWidth, int aHeight, Result &r)
{
	static bool usePng = false;
	if (usePng)
	{
		cv::Mat rawData = cv::Mat(1, aBufferLength, CV_8UC1, aBuffer);
		cv::imdecode(rawData, cv::IMREAD_UNCHANGED, &myDecodedScreenBuffer);
	}
	else
	{
		myDecodedScreenBuffer = cv::Mat(aWidth, aHeight, CV_8UC4, aBuffer);
	}

	if (myDecodedScreenBuffer.empty())
	{
		printf("Failed to decode buffer of length: %i\n", aBufferLength);
		return false;
	}

	if (AddScreenBGR(myDecodedScreenBuffer, TimeNow(), r))
	{
		return true;
	}
	return false;
}

bool Query::AddScreenBGR(const cv::Mat &aScreen, const int aCurrentTime, Result &oResult)
{
	UpdateSearchSettings();

	myIsAutoMatch = true;
	bool found = false;
	if (aScreen.channels() == 4)
	{
		cv::cvtColor(aScreen, myLastScreenBGR, CV_BGRA2BGR);
	}
	else
	{
		aScreen.copyTo(myLastScreenBGR);
	}

	if (myScreenScale < 1.f)
	{
		cv::resize(myLastScreenBGR, myLastScreenBGR, cv::Size(myLastScreenBGR.size().width * myScreenScale, myLastScreenBGR.size().height * myScreenScale));
	}

	if (myscreenHistory.size() > 0)
	{
		if (myscreenHistory[0].cols != myLastScreenBGR.cols || myscreenHistory[0].rows != myLastScreenBGR.rows)
		{
			ClearScreenHistory();
		}
	}

	cv::Mat smallMat;
	cv::resize(myLastScreenBGR, smallMat, cv::Size(myLastScreenBGR.size().width / 2, myLastScreenBGR.size().height / 2));

	cv::Mat lastGray;
	cv::cvtColor(smallMat, lastGray, CV_BGR2GRAY);

	const int currentTime = aCurrentTime;
	myscreentimes.push_back(currentTime);
	myscreenHistory.push_back(myLastScreenBGR);
	mygrayMiniHistory.push_back(lastGray);

	const int maxHistory = 5000;

	int i = 0;
	for (int e = (int)myscreentimes.size() - 1; i < e; ++i)
	{
		if (currentTime - myscreentimes[i] < maxHistory)
		{
			--i; //keep the last one that is above maxHistory
			break;
		}
	}
	if (i > 0)
	{
		myscreenHistory.erase(myscreenHistory.begin(), myscreenHistory.begin() + i);
		mygrayMiniHistory.erase(mygrayMiniHistory.begin(), mygrayMiniHistory.begin() + i);
		myscreentimes.erase(myscreentimes.begin(), myscreentimes.begin() + i);
	}

	if (myscreenHistory.size() > 1)
	{
		found = TestDiff(oResult, currentTime);
	}

	myIsAutoMatch = false;

	return found;
}

void Query::UpdateSearchSettings()
{
	UpdateScreenScale();
	mySettings.SetMinMax(myScreenScale * float(VIDEOHEIGHT) * myMinCardHeightRelative, myScreenScale * float(VIDEOHEIGHT) * myMaxCardHeightRelative);
}

void Query::UpdateScreenScale()
{
	float screenScale = 1.f;
	if (myMaxCardHeightRelative > myMinCardHeightRelative && myMinCardHeightRelative > 0.f)
	{
		const float minCardHeightAbsolute = 128.f;
		const float minCardHeight = float(VIDEOHEIGHT) * myMinCardHeightRelative;
		if (minCardHeight > minCardHeightAbsolute * 1.2f)
		{
			screenScale = minCardHeightAbsolute / minCardHeight;
		}
	}

	if (screenScale != myScreenScale)
		myScreenScale = screenScale;
}

bool Query::TestDiff(Result &oResult, const int aCurrentTime)
{
	if (mygrayMiniHistory.size() < 2)
	{

		return false;
	}

	if (myLastOkMatch && (aCurrentTime - myLastOkMatch < myAutoMatchTimeout))
	{
		return false;
	}

	const cv::Mat &oldGray = mygrayMiniHistory[0];
	const cv::Mat &previousGray = mygrayMiniHistory[mygrayMiniHistory.size() - 2];
	const cv::Mat &currentGray = mygrayMiniHistory[mygrayMiniHistory.size() - 1];
	const cv::Mat &currentGrayFullSize = myscreenHistory[myscreenHistory.size() - 1];

	cv::Mat threshold;
	const int morph_size = 1;
	cv::Mat element = getStructuringElement(cv::MORPH_RECT, cv::Size(2 * morph_size + 1, 2 * morph_size + 1), cv::Point(morph_size, morph_size));
	{
		static bool useCamaraCutDetection = false;
		static bool lowChangeDetection = false;
		{
			cv::Mat prediff;
			cv::absdiff(previousGray, currentGray, prediff);
			cv::threshold(prediff, prediff, 25, 255, cv::THRESH_BINARY);
			double diffsum = cv::sum(prediff)[0] / 255;
			double area = prediff.size().area();
			double percent = diffsum / area;

			if (lowChangeDetection && percent < 0.01)
			{
				return false;
			}

			if (useCamaraCutDetection && percent > 0.5)
			{
				ClearScreenHistory();
				return false;
			}
		}

		cv::Mat oldDiff;
		cv::absdiff(oldGray, currentGray, oldDiff);
		cv::threshold(oldDiff, oldDiff, 20, 255, cv::THRESH_BINARY);
		cv::dilate(oldDiff, threshold, cv::Mat(), cv::Point(-1, -1), 1);

		// 		cv::bitwise_and(prediff, oldDiff, threshold);
	}

	morphologyEx(threshold, threshold, MORPH_OPENING, element, cv::Point(-1, -1), 3);
	morphologyEx(threshold, threshold, MORPH_CLOSING, element, cv::Point(-1, -1), 3);
	cv::dilate(threshold, threshold, cv::Mat(), cv::Point(-1, -1), 5);

	cv::Mat t2;
	cv::cvtColor(threshold, t2, cv::COLOR_GRAY2BGR);

	std::vector<std::vector<cv::Point>> contours;
	std::vector<cv::Vec4i> hierarchy;
	cv::findContours(threshold, contours, hierarchy, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

	//grayscaled is half, resize it now
	for (std::vector<cv::Point> &contour : contours)
	{
		for (cv::Point &point : contour)
		{
			point.x *= 2.f;
			point.y *= 2.f;
		}
	}

	for (int i = 0, e = contours.size(); i < e; ++i)
	{
		const std::vector<cv::Point> &contour = contours[i];
		if (contour.size() < 4)
		{
			continue;
		}

		cv::Rect rect = cv::boundingRect(contour);
#ifdef DEBUGIMAGES
		//  		cv::RotatedRect rr = cv::minAreaRect(contour);
		cv::Mat dbMat(currentGrayFullSize, rect);
#endif
		const double rectArea = rect.size().area();
		if (rectArea < 1200 * myScreenScale)
		{
			continue;
		}

		if (mySettings.myHeightSet)
		{
			if (min(rect.size().width, rect.size().height) < HeightToWidth(mySettings.myMinCardHeight))
				continue;

			if (max(rect.size().width, rect.size().height) < mySettings.myMinCardHeight)
				continue;
		}

		cv::Mat rectImage(myLastScreenBGR, rect);

		myPoint.x = rect.x;
		myPoint.y = rect.y;

		if (FindCardInRoi(mySettings, myCardDatabase.myCardLists, rectImage, false, oResult))
		{
			while (oResult.myMatch.myList.size() > 0 && oResult.myMatch.myList[0].myScore[0] < myGoodMatchScore)
			{
				const CardData *matchedCard = oResult.myMatch.myList[0].myDatabaseCard;
				for (const CardData *previousCard : myAlreadyMatched)
				{
					if (previousCard->myCardName == matchedCard->myCardName)
					{
						matchedCard = nullptr;
						break;
					}
				}

				if (!matchedCard)
				{
					oResult.myMatch.myList.erase(oResult.myMatch.myList.begin()); //remove first
					continue;
				}

				if (matchedCard->myFormat & CardData::BASIC) //ignore basic lands for automatches
				{
					return false;
				}

				ClearScreenHistory();

				myAlreadyMatched.push_back(matchedCard);
				if (myAlreadyMatched.size() > myAlreadyMatchedMaxSize)
				{
					myAlreadyMatched.erase(myAlreadyMatched.begin());
				}

#ifdef DEBUGIMAGES
				oResult.myMatch.myCard.myPotenatialRects[oResult.myMatch.myList[0].myPotentialRectIndex].CreateBaseImage(rectImage);
#endif
				myLastOkMatch = aCurrentTime;
				return true;
			}
		}
	}
	return false;
}

const cv::Mat &Query::GetBGRScreen() const
{
	return myLastScreenBGR;
}

void MoveRectX(cv::RotatedRect &ioRect, const float iAmount)
{
	float xmove = cos(ioRect.angle * 3.14f / 180.f) * iAmount;
	float ymove = sin(ioRect.angle * 3.14f / 180.f) * iAmount;
	ioRect.center.x -= xmove;
	ioRect.center.y -= ymove;
}

cv::RotatedRect MakeCorrectWidth(cv::RotatedRect aRect)
{
	float widthDiff = Query::HeightToWidth(aRect.size.height) - aRect.size.width;
	aRect.size.width += widthDiff;
	MoveRectX(aRect, widthDiff / 2.f);
	return aRect;
}

void GetCorrectedWidthRects(cv::RotatedRect aRect, cv::RotatedRect &oLeftRect, cv::RotatedRect &oRightRect)
{
	float widthDiff = Query::HeightToWidth(aRect.size.height) - aRect.size.width;
	aRect.size.width += widthDiff;
	oLeftRect = aRect;
	MoveRectX(oLeftRect, widthDiff / 2.f);
	oRightRect = aRect;
	MoveRectX(oRightRect, -widthDiff / 2.f);
}

void MoveRectY(cv::RotatedRect &ioRect, float angle, const float iAmount)
{
	angle -= 90.f;
	float xmove = cos(angle * 3.14f / 180.f) * iAmount;
	float ymove = sin(angle * 3.14f / 180.f) * iAmount;
	ioRect.center.x += xmove;
	ioRect.center.y += ymove;
}

void GetCorrectedHeightRects(cv::RotatedRect aRect, const float anAngle, cv::RotatedRect &oTopRect, cv::RotatedRect &oBottomRect)
{
	float heightDiff = Query::WidthToHeight(aRect.size.width) - aRect.size.height;
	aRect.size.height += heightDiff;
	oTopRect = aRect;
	MoveRectY(oTopRect, anAngle, heightDiff / 2.f);
	oBottomRect = aRect;
	MoveRectY(oBottomRect, anAngle, -heightDiff / 2.f);
}

void AddRect(const PotentialRect &aRect, std::vector<PotentialRect> &oRects)
{
	oRects.push_back(aRect);
}

bool CreatePotentialRectFromPossibleTextBox(const float fullCardRatio, const cv::RotatedRect &rect, const std::string &path, std::vector<PotentialRect> &oPotentialRects)
{
	bool textBoxOnly = false;
	bool typeBarAlso = false;
	if (fullCardRatio >= 0.45f && fullCardRatio <= 0.52) //textbox only
	{
		textBoxOnly = true;
	}
	else if (fullCardRatio >= 0.58f && fullCardRatio <= 0.64) //include type bar
	{
		typeBarAlso = true;
	}

	if (textBoxOnly || typeBarAlso)
	{
		cv::RotatedRect testRect = rect;
		std::string newPath = path;
		if (testRect.size.height > testRect.size.width)
		{
			std::swap(testRect.size.width, testRect.size.height);
			testRect.angle += 90.0;
			newPath += "+90.";
		}

		cv::RotatedRect correctedRect = testRect;
		float oldHeight = testRect.size.height;
		float middleToTop = testRect.size.width * 1.55f - oldHeight / 2.f;
		if (textBoxOnly)
		{
			middleToTop *= 1.1f;
		}
		testRect.size.width *= 1.086f;
		testRect.size.height = Query::WidthToHeight(testRect.size.width);

		cv::RotatedRect topRect = testRect;
		MoveRectY(topRect, testRect.angle, middleToTop / 3.f);
		cv::RotatedRect bottomRect = testRect;
		MoveRectY(bottomRect, testRect.angle, -middleToTop / 3.f);

#ifdef DEBUGIMAGES
		cv::Mat rectMat = getRotatedRectImage(rect, debugArea);
		cv::Mat testRectMat = getRotatedRectImage(testRect, debugArea);
		cv::Mat topRectMat = getRotatedRectImage(topRect, debugArea);
		cv::Mat bottomRectMat = getRotatedRectImage(bottomRect, debugArea);
#endif

		AddRect(PotentialRect(topRect, newPath + ".topRect"), oPotentialRects);
		AddRect(PotentialRect(bottomRect, newPath + ".botRect"), oPotentialRects);
		return true;
	}
	return false;
}

void addPotentialRects(const SearchSettings &inputs, cv::RotatedRect &rect, const std::string &path, const std::vector<cv::Point> &contour, std::vector<PotentialRect> &oPotentialRects)
{
	// 		if (inputs.myHeightSet)
	{
		if (inputs.IsValidWidthPermissive(rect.size.width))
		{
			cv::RotatedRect topRect, bottomRect;
			GetCorrectedHeightRects(rect, rect.angle, topRect, bottomRect);
#ifdef DEBUGIMAGES
			cv::Mat topRectMat = getRotatedRectImage(topRect, debugArea);
			cv::Mat bottomRectMat = getRotatedRectImage(bottomRect, debugArea);
#endif
			AddRect(PotentialRect(topRect, path + ".wpwTop", contour), oPotentialRects);
			AddRect(PotentialRect(bottomRect, path + ".wpwBot", contour), oPotentialRects);
		}

		if (inputs.IsValidHeightPermissive(rect.size.height))
		{
			cv::RotatedRect leftRect, rightRect;
			GetCorrectedWidthRects(rect, leftRect, rightRect);
#ifdef DEBUGIMAGES
			cv::Mat leftRectMat = getRotatedRectImage(leftRect, debugArea);
			cv::Mat rightRectMat = getRotatedRectImage(rightRect, debugArea);
#endif
			AddRect(PotentialRect(leftRect, path + ".hphLeft", contour), oPotentialRects);
			AddRect(PotentialRect(rightRect, path + ".hphRight", contour), oPotentialRects);
		}

		if (inputs.IsValidWidthPermissive(rect.size.height))
		{
			cv::RotatedRect modified = rect;
			std::swap(modified.size.width, modified.size.height);
			modified.angle += 90.0;

			cv::RotatedRect topRect, bottomRect;
			GetCorrectedHeightRects(modified, rect.angle, topRect, bottomRect);
#ifdef DEBUGIMAGES
			cv::Mat topRectMat = getRotatedRectImage(topRect, debugArea);
			cv::Mat bottomRectMat = getRotatedRectImage(bottomRect, debugArea);
#endif
			AddRect(PotentialRect(topRect, path + ".wphTop", contour), oPotentialRects);
			AddRect(PotentialRect(bottomRect, path + ".wphBot", contour), oPotentialRects);
		}

		if (inputs.IsValidHeightPermissive(rect.size.width))
		{
			cv::RotatedRect modified = rect;
			std::swap(modified.size.width, modified.size.height);
			modified.angle += 90.0;
			cv::RotatedRect leftRect, rightRect;
			GetCorrectedWidthRects(modified, leftRect, rightRect);
#ifdef DEBUGIMAGES
			cv::Mat leftRectMat = getRotatedRectImage(leftRect, debugArea);
			cv::Mat rightRectMat = getRotatedRectImage(rightRect, debugArea);
#endif
			AddRect(PotentialRect(leftRect, path + ".hpwLeft", contour), oPotentialRects);
			AddRect(PotentialRect(rightRect, path + ".hpwRight", contour), oPotentialRects);
		}
	}
}

void FindCardRectsInContours(const std::vector<std::vector<cv::Point>> &iContours, const SearchSettings inputs, const cv::Mat &aroundCardArea, std::vector<PotentialRect> &oGoodRects, std::vector<PotentialRect> *oPotentialRects, const std::string &path)
{
	const double areaarea = aroundCardArea.size().area();
	static std::vector<cv::Point> hull;
	hull.clear();

	for (const std::vector<cv::Point> &contour : iContours)
	{
		if (contour.size() < 4)
		{
			continue;
		}

		cv::Rect bounds = cv::boundingRect(contour);
#ifdef DEBUGIMAGES
		cv::Mat boundsImage(aroundCardArea, bounds);
#endif
		if (bounds.size().area() < 400.f)
		{
			continue;
		}

		cv::RotatedRect rect = cv::minAreaRect(contour);

		double rectArea = rect.size.area();
		if (rectArea < 400.f)
		{
			continue;
		}

		if (rect.size.width < 20.f)
		{
			continue;
		}
		if (rect.size.height < 20.f)
		{
			continue;
		}

		static bool guaranteeNotFullImage = false;
		if (guaranteeNotFullImage)
		{
			if ((areaarea / rectArea) < 2.f)
			{
				continue;
			}
		}

		cv::convexHull(contour, hull);
		double hullarea = cv::contourArea(hull);
		double contourarea = cv::contourArea(contour);
		double rectness = hullarea / rectArea;
#ifdef DEBUGIMAGES
		std::vector<cv::Point> approx;
		cv::approxPolyDP(cv::Mat(contour), approx, cv::arcLength(cv::Mat(contour), true) * 0.01, true);

		cv::Mat drawnDebugArea = debugArea.clone();
		for (int i = 0, e = contour.size(); i < e - 1; ++i)
		{
			cv::line(drawnDebugArea, contour[i],
					 contour[i + 1], cv::Scalar(0, 0, 255), 1, 8);
		}
		for (int i = 0, e = hull.size(); i < e - 1; ++i)
		{
			cv::line(drawnDebugArea, hull[i],
					 hull[i + 1], cv::Scalar(0, 255, 0), 1, 8);
		}
		for (int i = 0, e = approx.size(); i < e - 1; ++i)
		{
			cv::line(drawnDebugArea, approx[i],
					 approx[i + 1], cv::Scalar(255, 0, 0), 1, 8);
		}

		cv::Mat debugRect = getRotatedRectImage(rect, drawnDebugArea);
#endif

		float fullCardRatio = GetRectRatio(rect);
		if (fullCardRatio < 0.1f) //can't do much with a sliver
		{
			continue;
		}

		static bool useTextBox = true;
		if (useTextBox && rectness >= 0.95)
		{
			CreatePotentialRectFromPossibleTextBox(fullCardRatio, rect, path, oGoodRects);
		}
		// 		else
		// 		{
		// 			continue;
		// 		}

		if (inputs.myHeightSet)
		{
			const float estimatedTargetArea = inputs.myMinCardHeight * Query::HeightToWidth(inputs.myMinCardHeight);
			if (rectArea / estimatedTargetArea < 0.25f)
			{
				continue;
			}
		}

		float ratioDiff = PerfectRatioDiff(fullCardRatio);
		float scaledWidth = rect.size.width;
		if (ratioDiff > 0.1f)
		{
			scaledWidth = Query::HeightToWidth(rect.size.height);
		}

		if (rectness > 0.95f && ratioDiff < 0.1f) // && fmod(rect.angle, 90.f)<1.f)
		{
			if (rect.size.width > rect.size.height)
			{
				std::swap(rect.size.width, rect.size.height);
				rect.angle += 90.0;
			}
			AddRect(PotentialRect(rect, path, contour), oGoodRects);
			continue;
		}

		if (oPotentialRects)
		{
			addPotentialRects(inputs, rect, path, contour, *oPotentialRects);
		}
	}
}

void FindCardRects(const char *path, const cv::Mat &grayBlurredEqualized, const SearchSettings inputs, const cv::Mat &aroundCardArea, std::vector<PotentialRect> &oGoodRects, std::vector<PotentialRect> *oPotentialRects)
{
	int thresholds[6] = {20, 40, 60, 80, 190, 220}; //in some sort of likelieness

	std::vector<cv::Mat1b> thresholdMats;

	for (const int tValue : thresholds)
	{
		cv::Mat1b currentThreshold;
		cv::threshold(grayBlurredEqualized, currentThreshold, tValue, 255, CV_THRESH_BINARY);
		thresholdMats.push_back(currentThreshold.clone());

		cv::Mat1b otherThreshold;
		cv::bitwise_not(currentThreshold, otherThreshold);
		thresholdMats.push_back(otherThreshold.clone());
	}

	std::vector<std::vector<cv::Point>> contours;
	std::vector<cv::Vec4i> hierarchy;
	char buffer[33];
	buffer[0] = 0;
	for (int i = 0, e = (int)thresholdMats.size(); i < e; ++i)
	{
		const cv::Mat1b &areaThreshold = thresholdMats[i];
		cv::findContours(areaThreshold.clone(), contours, hierarchy, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

		if (path)
		{
			sprintf(buffer, "%s.%d", path, i);
		}
		FindCardRectsInContours(contours, inputs, aroundCardArea, oGoodRects, oPotentialRects, buffer);
	}
}

void FindMouseRects(const char *path, cv::Mat1b grayArea, const SearchSettings inputs, const cv::Mat &aRoiMat, std::vector<PotentialRect> &oGoodRects, std::vector<PotentialRect> *oPotentialRects)
{
	FindCardRects(path, grayArea, inputs, aRoiMat, oGoodRects, oPotentialRects);

	static std::vector<std::vector<cv::Point>> contours;
	contours.clear();

	static std::vector<cv::Vec4i> hierarchy;
	hierarchy.clear();

	char buffer[33];
	buffer[0] = 0;
	if (inputs.myUseCanny)
	{
		static cv::Mat1b canny;
		cv::Mat1b clonedArea = grayArea;
		for (int i = 10; i <= 90; i += 20)
		{
			cv::Canny(clonedArea, canny, i, i * 3, 3, true);
			cv::findContours(canny, contours, hierarchy, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

			if (path)
			{
				sprintf(buffer, "%s.canny%d", path, i);
			}
			FindCardRectsInContours(contours, inputs, aRoiMat, oGoodRects, oPotentialRects, buffer);
		}
	}
}

float GetMeanPerfectRectHeight(const std::vector<PotentialRect> &potentialRects)
{
	std::vector<float> perfectHeights;
	for (const PotentialRect &pRect : potentialRects)
	{
		const float fullCardRatio = pRect.GetRatio();
		const float ratioDiff = PerfectRatioDiff(fullCardRatio);
		if (ratioDiff < 0.03f)
		{
			perfectHeights.push_back(max(pRect.myRotatedRect.size.width, pRect.myRotatedRect.size.height));
		}
	}

	if (!perfectHeights.size())
	{
		return -1;
	}

	std::sort(perfectHeights.begin(), perfectHeights.end());
	if (perfectHeights.size() % 2 == 1)
	{
		return perfectHeights[perfectHeights.size() / 2];
	}

	while (perfectHeights.size() > 2)
	{
		perfectHeights.pop_back();
		if (perfectHeights.size() > 2)
		{
			perfectHeights.erase(perfectHeights.begin());
		}
	}

	float sum = 0.f;
	for (const float height : perfectHeights)
	{
		sum += height;
	}
	float mean = sum / float(perfectHeights.size());
	// 	float diffsum = 0.f;
	// 	for (const float height : perfectHeights)
	// 	{
	// 		float diff = height - mean;
	// 		diffsum += (diff*diff);
	// 	}
	// 	float stddev = diffsum / float(perfectHeights.size());

	return mean;
}

void GetPossibleCards(const std::vector<PotentialRect> &someRects, std::vector<PotentialCardMatches> &oPotentialCards)
{
	oPotentialCards.clear();
	for (const PotentialRect &pRect : someRects)
	{
		bool added = false;
		for (PotentialCardMatches &matchList : oPotentialCards)
		{
			if (matchList.myCard.Matches(pRect))
			{
				matchList.myCard.Add(pRect);
				added = true;
				break;
			}
		}
		if (!added)
		{
			oPotentialCards.push_back(pRect);
		}
	}
}

bool Query::FindCardInRoi(SearchSettings &inputs, const std::vector<const CardList *> &iCardSets, const cv::Mat &aRoiMat, const bool useCenterFilter, Result &oResult)
{

	cv::Mat1b grayArea;
	cv::cvtColor(aRoiMat, grayArea, cv::COLOR_BGR2GRAY);
	debugArea = grayArea;

	static std::vector<PotentialRect> goodRects;
	goodRects.clear();

	static std::vector<PotentialRect> potentialRects;
	potentialRects.clear();

	FindMouseRects("", grayArea, inputs, grayArea, goodRects, (useCenterFilter) ? &potentialRects : nullptr);

	cv::Mat eqMat;
	cv::equalizeHist(grayArea, eqMat);

	FindMouseRects("eq", eqMat, inputs, grayArea, goodRects, (useCenterFilter) ? &potentialRects : nullptr);

	if (goodRects.size() > 0)
	{
		const float mean = GetMeanPerfectRectHeight(goodRects);
		if (mean > 48.f)
		{
			if (!inputs.myHeightSet || (myIsAutoMatch && inputs.IsValidHeightPermissive(mean)))
			{
				inputs.SetMeanHeight(mean);
			}
		}
	}

	static bool useCardHeightFilter = false;
	if (useCardHeightFilter && inputs.myHeightSet)
	{
		potentialRects.erase(std::remove_if(potentialRects.begin(), potentialRects.end(), [&](const PotentialRect &aRect) {
								 bool valid = inputs.IsValidPermissive(aRect.myRotatedRect);
								 return !valid;
							 }),
							 potentialRects.end());

		goodRects.erase(std::remove_if(goodRects.begin(), goodRects.end(), [&](const PotentialRect &aRect) {
							bool valid = inputs.IsValidPermissive(aRect.myRotatedRect);
#ifdef DEBUGIMAGES
							if (!valid)
							{
								PotentialRect test = aRect;
								test.CreateBaseImage(grayArea);
							}
#endif
							return !valid;
						}),
						goodRects.end());
	}

	static bool useRotationFilter = true;
	if (useRotationFilter && myMaxRotation > 0.f) //remove angled anything
	{
		potentialRects.erase(std::remove_if(potentialRects.begin(), potentialRects.end(), [&](const PotentialRect &aRect) {
								 bool valid = inputs.IsValidPermissive(aRect.myRotatedRect);
								 bool atAngle = fmod(aRect.myRotatedRect.angle, 90.f) > myMaxRotation;
								 return atAngle && !valid;
							 }),
							 potentialRects.end());

		goodRects.erase(std::remove_if(goodRects.begin(), goodRects.end(), [&](const PotentialRect &aRect) {
							bool valid = inputs.IsValidPermissive(aRect.myRotatedRect);
							bool atAngle = fmod(aRect.myRotatedRect.angle, 90.f) > myMaxRotation;
							return atAngle && !valid;
						}),
						goodRects.end());
	}

	if (useCenterFilter)
	{
		const int centerX = aRoiMat.cols / 2;
		const int centerY = aRoiMat.rows / 2;

		auto pointOutsideRect = [&](const PotentialRect &aRect) {
			return !PointInRect(aRect, centerX, centerY);
		};
		potentialRects.erase(std::remove_if(potentialRects.begin(), potentialRects.end(), pointOutsideRect), potentialRects.end());
		goodRects.erase(std::remove_if(goodRects.begin(), goodRects.end(), pointOutsideRect), goodRects.end());
	}

	std::vector<PotentialCardMatches> goodCards;
	GetPossibleCards(goodRects, goodCards);

	std::vector<PotentialCardMatches> rectCards;
	GetPossibleCards(potentialRects, rectCards);

	auto cardSort = [](const PotentialCardMatches &lhs, const PotentialCardMatches &rhs) -> bool {
		return lhs.myCard.myPotenatialRects.size() > rhs.myCard.myPotenatialRects.size();
	};

	std::sort(goodCards.begin(), goodCards.end(), cardSort);
	std::sort(rectCards.begin(), rectCards.end(), cardSort);

	int goodVariations = 0;
	for (const PotentialCardMatches &pMatch : goodCards)
	{
		goodVariations += (int)pMatch.myCard.myPotenatialRects.size();
	}

	int rectVariations = 0;
	for (const PotentialCardMatches &pMatch : rectCards)
	{
		rectVariations += (int)pMatch.myCard.myPotenatialRects.size();
	}

	if (goodVariations + rectVariations > 30)
	{
		rectCards.erase(std::remove_if(rectCards.begin(), rectCards.end(), [&](const PotentialCardMatches &aCard) {
							return aCard.myCard.myPotenatialRects.size() < 3;
						}),
						rectCards.end());
	}

	int cardSum = 0;
	for (const CardList *cardSet : iCardSets)
	{
		cardSum += (int)cardSet->myCardData.size();
	}

	bool hasMatch = false;

	hasMatch = FindCardInMouseRects(goodCards, inputs, iCardSets, grayArea, oResult);

	if (hasMatch && oResult.myMatch.myList[0].myScore[0] < myGoodMatchScore)
	{
		if (useCenterFilter)
		{
			myAlreadyMatched.push_back(oResult.myMatch.myList[0].myDatabaseCard);
		}
		return true;
	}

	if (potentialRects.size() > 0)
	{
		hasMatch = FindCardInMouseRects(rectCards, inputs, iCardSets, grayArea, oResult);

		if (hasMatch && oResult.myMatch.myList[0].myScore[0] < myOkMatchScore)
		{
			if (useCenterFilter)
			{
				myAlreadyMatched.push_back(oResult.myMatch.myList[0].myDatabaseCard);
			}
			return true;
		}
	}

	return hasMatch && oResult.myMatch.myList[0].myScore[0] < myOkMatchScore;
}
cv::Mat GetScaled(const cv::Mat &image, float xScale, float yScale)
{
	int newWidth = image.cols * xScale;
	int newHeight = image.rows * yScale;
	return cv::Mat(image, cv::Rect((image.cols - newWidth) / 2, (image.rows - newHeight) / 2, newWidth, newHeight));
}

bool HasVerticalColorLine(cv::Mat &image)
{
	for (int x = image.cols / 10; x < image.cols - image.cols / 10; ++x)
	{
		bool hasLine = true;
		const short baseValue = image.at<uchar>(0, x);
		for (int y = image.rows / 10; y < image.rows - image.rows / 10; y++)
		{
			const short currentValue = image.at<uchar>(y, x);

			const double distance = cv::norm(currentValue - baseValue);
			if (distance > 3)
			{
				hasLine = false;
				break;
			}
		}
		if (hasLine)
		{
			return true;
		}
	}
	return false;
}

void GetRectVariations(const std::string &inputPath, const cv::Mat &largeImage, const cv::RotatedRect &aRect, bool shouldFlip, std::vector<CardInput> &oCardList)
{
	const char *pathStr = "";

	std::string path;
	{
		cv::Mat image = GetScaled(largeImage, 0.9f, 0.9f); //"real" match
		static bool lineValidation = true;
		if (lineValidation)
		{
			if (HasVerticalColorLine(image))
			{
				return; //broken image, don't care about this
			}
		}
#ifdef DEBUGIMAGES
		path = inputPath + "real";
		pathStr = path.c_str();
#endif
		GrabFullCardFlips(image, aRect, oCardList, shouldFlip, pathStr);
	}
	{
		cv::Mat evenSmaller = GetScaled(largeImage, 0.84f, 0.84f);
#ifdef DEBUGIMAGES
		path = inputPath + ".84";
		pathStr = path.c_str();
#endif
		GrabFullCardFlips(evenSmaller, aRect, oCardList, shouldFlip, pathStr);
	}
	{
		cv::Mat evenSmaller = GetScaled(largeImage, 0.76f, 0.76f);
#ifdef DEBUGIMAGES
		path = inputPath + ".76";
		pathStr = path.c_str();
#endif
		GrabFullCardFlips(evenSmaller, aRect, oCardList, shouldFlip, pathStr);
	}

	{
		cv::Mat slightlySmaller = GetScaled(largeImage, 0.88f, 0.88f);
#ifdef DEBUGIMAGES
		path = inputPath + ".88";
		pathStr = path.c_str();
#endif
		GrabFullCardFlips(slightlySmaller, aRect, oCardList, shouldFlip, pathStr);
	}

	{
		cv::Mat textCut2 = GetScaled(largeImage, 0.92f, 0.9f);
#ifdef DEBUGIMAGES
		path = inputPath + ".textcut2";
		pathStr = path.c_str();
#endif
		GrabFullCardFlips(textCut2, aRect, oCardList, shouldFlip, pathStr);
	}

	{
		cv::Mat slightlyLarger = GetScaled(largeImage, 0.94f, 0.94f);
#ifdef DEBUGIMAGES
		path = inputPath + ".94";
		pathStr = path.c_str();
#endif
		GrabFullCardFlips(slightlyLarger, aRect, oCardList, shouldFlip, pathStr);
	}

	{
		cv::Mat slightlyLarger = GetScaled(largeImage, 0.92f, 0.92f);
#ifdef DEBUGIMAGES
		path = inputPath + ".92";
		pathStr = path.c_str();
#endif
		GrabFullCardFlips(slightlyLarger, aRect, oCardList, shouldFlip, pathStr);
	}
}

bool Query::FindCardInMouseRects(std::vector<PotentialCardMatches> &iPotentialCards, const SearchSettings &inputs, const std::vector<const CardList *> &iCardSets, const cv::Mat &aroundCardArea, Result &oResult)
{
	// 	for (int i = 0, e = (int)iPotentialCards.size(); i < e; ++i)
	// 	{
	// 		for (PotentialRect& potentialRect : iPotentialCards[i].myCard.mypotenatialRects)
	// 		{
	// 			if (!potentialRect.myvariations.size())
	// 			{
	// 				potentialRect.CreateBaseImage(aroundCardArea, inputs);
	// 			}
	// 		}
	// 	}

	int before = TimeNow();
	bool res = FindBestMatch(iPotentialCards, iCardSets, inputs, aroundCardArea, oResult);

	//	std::cout << "[BestMatch " << (TimeNow() - before) << "ms]";

	return res;
}

cv::Point2f RotateVector(cv::Point2f vec, const float deg)
{
	const float rad = deg * (3.14159f / 180.f);
	cv::Point2f ret;
	ret.x = vec.x * cos(rad) - vec.y * sin(rad);
	ret.y = vec.x * sin(rad) + vec.y * cos(rad);
	return ret;
}

cv::RotatedRect GetRotatedRectFromSelectedLine(int ax, int ay, int bx, int by, const float smallAngle, const float largeAngle, const float angleOffset)
{
	cv::Point2f vec(bx - ax, by - ay);
	const float len = std::sqrt(vec.x * vec.x + vec.y * vec.y);
	vec.x /= len;
	vec.y /= len;
	const float cardHeight = cos(std::abs(smallAngle) * (3.14159f / 180.f)) * len; //cos v = n�r/hyp?
	const float cardWidth = sin(std::abs(smallAngle) * (3.14159f / 180.f)) * len;  //sin v = mot/hyp'

	cv::Point2f widthVec = RotateVector(vec, -largeAngle + angleOffset);
	cv::Point2f heightVec = RotateVector(vec, smallAngle + angleOffset);

	std::vector<cv::Point2f> rectCoords;
	rectCoords.push_back(cv::Point2f((float)ax, (float)ay));
	rectCoords.push_back(cv::Point2f((float)bx, (float)by));
	rectCoords.push_back(cv::Point2f((float)(ax + widthVec.x * cardWidth), (float)(ay + widthVec.y * cardWidth)));
	rectCoords.push_back(cv::Point2f((float)(ax + heightVec.x * cardHeight), (float)(ay + heightVec.y * cardHeight)));
	return cv::minAreaRect(rectCoords);
}

void Query::Log(const char *aLog) const
{
	if (mylog)
	{
		mylog(aLog);
	}
}

void AddAndAverage(cv::RotatedRect &lhs, const cv::RotatedRect &rhs)
{
	lhs.angle = (lhs.angle + rhs.angle) / 2.f;
	lhs.size.width = (lhs.size.width + rhs.size.width) / 2.f;
	lhs.size.height = (lhs.size.height + rhs.size.height) / 2.f;
	lhs.center.x = (lhs.center.x + rhs.center.x) / 2.f;
	lhs.center.y = (lhs.center.y + rhs.center.y) / 2.f;
}

void SearchSettings::SetMeanHeight(float aMeanCardHeight)
{
	SetMinMax(aMeanCardHeight * 0.8f, aMeanCardHeight * 1.2f);
}

void SearchSettings::SetMinMax(float aMinCardHeight, float aMaxCardHeight)
{
	myMinCardHeight = aMinCardHeight;
	myMaxCardHeight = aMaxCardHeight;
	myHeightSet = true;
}

bool SearchSettings::IsValidPermissive(const cv::RotatedRect &aRect) const
{
	bool valid = !myHeightSet || (IsValidWidthPermissive(Width(aRect)) && IsValidHeightPermissive(Height(aRect)));
	return valid;
}

bool SearchSettings::IsValidWidthPermissive(const float aWidth) const
{
	return !myHeightSet || (aWidth >= Query::HeightToWidth(myMinCardHeight) && aWidth <= Query::HeightToWidth(myMaxCardHeight));
}

bool SearchSettings::IsValidHeightPermissive(const float aHeight) const
{
	return !myHeightSet || (aHeight >= myMinCardHeight && aHeight <= myMaxCardHeight);
}

PotentialRect::PotentialRect(cv::RotatedRect aRotatedRect, const std::string &aPath, const std::vector<cv::Point> &aContour) : myRotatedRect(aRotatedRect), myContour(aContour)
#ifdef DEBUGIMAGES
																															   ,
																															   path(aPath)
#endif
{
#if _WIN32
	if (aRotatedRect.size.width > aRotatedRect.size.height)
	{
		int *crash = nullptr;
		*crash = 1;
	}
#endif
}

PotentialRect::PotentialRect(cv::RotatedRect aRotatedRect, const std::string &aPath)
	: myRotatedRect(aRotatedRect)
#ifdef DEBUGIMAGES
	  ,
	  path(aPath)
#endif
{
#if _WIN32
	if (aRotatedRect.size.width > aRotatedRect.size.height)
	{
		int *crash = nullptr;
		*crash = 1;
	}
#endif
}

void PotentialRect::CreateBaseImage(const cv::Mat &area)
{
	const float rectArea = myRotatedRect.size.area();
#ifdef DEBUGIMAGES
	cv::Mat debugRect = getRotatedRectImage(myRotatedRect, area);
#endif
	const cv::RotatedRect &rect = myRotatedRect;

	cv::RotatedRect modified = rect;
	{
		modified.size.height *= 1.11f; //0.9% is the real deal
		modified.size.width *= 1.11f;

		cv::Mat largeImage = getRotatedRectImage(modified, area);
		const bool generateUpsideDown = true; //myIsPaper;
		GetRectVariations("", largeImage, rect, generateUpsideDown, myVariations);
		GetRectVariations("", GetScaled(largeImage, 0.94f, 0.99f), rect, generateUpsideDown, myVariations);

		for (int x = 0, xe = (int)myVariations.size(); x < xe; ++x)
		{
			CardInput &input = myVariations[x];
			CardData &queryCard = input.myQuery;
			queryCard.MakeHash();
		}
	}
}

float PotentialRect::GetRatio() const
{
	return GetRectRatio(myRotatedRect);
}
