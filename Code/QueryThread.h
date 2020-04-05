#pragma once
#include <thread>
#include "CardData.h"
#include <mutex>
#include <functional>
#include "CardDatabase.h"

struct CardData;
// #define DEBUGIMAGES

typedef std::function<void(const char *aLog)> LogFunction;

bool IsValidImageRatio(int cols, int rows);
struct SearchSettings
{
	SearchSettings() : myHeightSet(false), myUseCanny(true) {}
	void SetMeanHeight(float aMeanCardHeight);
	void SetMinMax(float aMinCardHeight, float aMaxCardHeight);
	bool IsValidPermissive(const cv::RotatedRect &aRect) const;
	bool IsValidWidthPermissive(const float aWidth) const;
	bool IsValidHeightPermissive(const float aHeight) const;

	float myMinCardHeight;
	float myMaxCardHeight;
	bool myHeightSet;

	const bool myUseCanny;
};
struct PotentialRect
{
	PotentialRect() {}
	PotentialRect(cv::RotatedRect aRotatedRect, const std::string &aPath, const std::vector<cv::Point> &aContour);
	PotentialRect(cv::RotatedRect aRotatedRect, const std::string &aPath);

	void CreateBaseImage(const cv::Mat &area);
	cv::RotatedRect myRotatedRect;
#ifdef DEBUGIMAGES
	std::string path;
#endif // DEBUGIMAGE
	std::vector<cv::Point> myContour;
	float GetRatio() const;
	std::vector<CardInput> myVariations;
};

struct PotentialCard
{
	PotentialCard() {}
	explicit PotentialCard(const PotentialRect &aPotentialRect)
	{
		myCenter = aPotentialRect.myRotatedRect.center;
		myAngle = aPotentialRect.myRotatedRect.angle;
		myPotenatialRects.push_back(aPotentialRect);
	}

	bool Matches(const PotentialRect &aPotentialRect) const
	{
		double distance = cv::norm(myCenter - aPotentialRect.myRotatedRect.center);
		double angleDist = cv::norm(myAngle - aPotentialRect.myRotatedRect.angle);
		return distance < 10 && angleDist < 2;
	}

	void Add(const PotentialRect &aPotentialRect)
	{
		//compute average after add
		const float sizeFloat = (float)myPotenatialRects.size();
		myCenter.x = (myCenter.x * sizeFloat + aPotentialRect.myRotatedRect.center.x) / (sizeFloat + 1.f);
		myCenter.y = (myCenter.y * sizeFloat + aPotentialRect.myRotatedRect.center.y) / (sizeFloat + 1.f);
		myAngle = (myAngle * sizeFloat + aPotentialRect.myRotatedRect.angle) / (sizeFloat + 1.f);

		auto it = std::find_if(myPotenatialRects.begin(), myPotenatialRects.end(),
							   [&](const PotentialRect &aListRect) {
								   float centerDiff = (float)cv::norm(aListRect.myRotatedRect.center - aPotentialRect.myRotatedRect.center);
								   int widthDiff = (int)cv::norm(aListRect.myRotatedRect.size.width - aPotentialRect.myRotatedRect.size.width);
								   int heightDiff = (int)cv::norm(aListRect.myRotatedRect.size.height - aPotentialRect.myRotatedRect.size.height);
								   float angleDiff = (float)cv::norm(aListRect.myRotatedRect.angle - aPotentialRect.myRotatedRect.angle);
								   return angleDiff < 0.01f && centerDiff < 1.f && widthDiff < 1 && heightDiff < 1;
							   });
		if (it == myPotenatialRects.end())
		{
			myPotenatialRects.push_back(aPotentialRect);
		}
	}

	cv::Point2f myCenter;
	float myAngle;
	std::vector<PotentialRect> myPotenatialRects;
};

struct PotentialCardMatches
{
	PotentialCardMatches() : myScore(1024) {}
	PotentialCardMatches(const PotentialCard &aCard) : myCard(aCard), myScore(1024) {}
	PotentialCardMatches(const PotentialRect &aCardRect) : myCard(aCardRect), myScore(1024) {}
	PotentialCard myCard;
	std::vector<Match> myList;
	float myScore;
};

typedef std::function<void(int aTotal, cv::Mat anInput)> QueryStartedFunction;
typedef std::function<void(const PotentialCardMatches &someMatches)> QueryDoneFunction;

bool GetMTGOCard(const cv::Mat &anInput, int px, int py, cv::Rect &gameArea);

static int TimeNow()
{
	return (int)(std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1));
}

struct Result
{
	Result() {}
	Result(const PotentialCardMatches &aBestMatch, cv::Point2i aSearchPoint)
		: myMatch(aBestMatch), myPoint(aSearchPoint)
	{
	}
	bool close(cv::Point2i aCurrentPoint) const
	{
		if (cv::norm(aCurrentPoint - myPoint) < 20.f)
		{
			return true;
		}
		return false;
	}

	PotentialCardMatches myMatch;
	cv::Point2i myPoint;
};

class Query
{
public:
	explicit Query(CardDatabase &aCardDatabase);

	std::string TestBuffer(unsigned char *aBuffer, int aWidth, int aHeight);
	bool TestFile(const std::string &file, bool aMatchingNameOnly = false);
	int SetSetting(const std::string &key, const std::string &value);

	static float HeightToWidth(float aHeight)
	{
		return (aHeight / 88.f) * 63.f;
	}
	static float WidthToHeight(float aWidth)
	{
		return (aWidth / 63.f) * 88.f;
	}
	bool FindBestMatch(std::vector<PotentialCardMatches> &underMouseCards, const std::vector<const CardList *> &iCardSets, const SearchSettings &inputs, const cv::Mat &aroundCardArea, Result &oResult);

	int FindPotentialRectMatches(PotentialRect &aPotentialRect, const std::vector<const CardList *> &iCardSets, int aWorst, std::vector<Match> &oMatchList);

	bool FindCardInRoiAndPrint(uint8_t *aBuffer, int aBufferLength, int aWidth, int aHeight, Result &r);
	bool FindCardInRoiAndPrint(const cv::Mat &source, const std::vector<const CardList *> &iCardSets, Result &r);

	bool AddScreenAndPrint(uint8_t *aBuffer, int aBufferLength, int aWidth, int aHeight, Result &r);
	bool AddScreenBGR(const cv::Mat &aScreen, int aCurrentTime, Result &oResult);

	void UpdateSearchSettings();

	void UpdateScreenScale();

	const cv::Mat &GetBGRScreen() const;
	std::vector<const CardData *> myAlreadyMatched;
	int myAlreadyMatchedMaxSize;
	int myLastOkMatch;
	cv::Mat myDecodedScreenBuffer;
	SearchSettings mySettings;
	float myMinCardHeightRelative;
	float myMaxCardHeightRelative;

	int myAutoMatchTimeout;
	int myGoodMatchScore;
	int myOkMatchScore;
	bool myIsAutoMatch;
	float myScreenScale;

	bool TestDiff(Result &oResult, const int aCurrentTime);

	bool FindCardInRoi(SearchSettings &inputs, const std::vector<const CardList *> &iCardSets, const cv::Mat &aRoiMat, bool useCenterFilter, Result &oResult);

	bool FindCardInMouseRects(std::vector<PotentialCardMatches> &underMouseRects, const SearchSettings &inputs, const std::vector<const CardList *> &iCardSets, const cv::Mat &aroundCardArea, Result &oResult);

	void Log(const char *aLog) const;

	LogFunction mylog;

	std::string testname;
	cv::Mat myLastScreenBGR;

	void ClearScreenHistory() //clears all but the last ones
	{
		myscreentimes.erase(myscreentimes.begin(), myscreentimes.end() - 1);
		myscreenHistory.erase(myscreenHistory.begin(), myscreenHistory.end() - 1);
		mygrayMiniHistory.erase(mygrayMiniHistory.begin(), mygrayMiniHistory.end() - 1);
	}

	std::vector<cv::Mat> myscreenHistory;
	std::vector<cv::Mat> mygrayMiniHistory;
	std::vector<int> myscreentimes;

	Result myLastResult;

	const float myMaxRotation;
	cv::Point2i myPoint;

	int myToplistSize;
	CardDatabase &myCardDatabase;
};
