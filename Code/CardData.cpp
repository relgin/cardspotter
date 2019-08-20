#include "CardData.h"
#include "QueryThread.h"
#include <bitset>
#include <opencv2/imgproc/types_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <opencv2/imgcodecs.hpp>

const char* colorString = { "rgbuwc" };

#ifdef _WIN32
#include <Windows.h>
int getFileNames(const char* folder, std::vector<std::string>& filenames)
{
	char search_path[200];
	sprintf(search_path, "%s/*.*", folder);
	std::string stringFolder(folder);
	stringFolder += "/";

	WIN32_FIND_DATA fd;
	HANDLE hFind = ::FindFirstFile(search_path, &fd);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			// read all (real) files in current folder
			// , delete '!' read other 2 default folder . and ..
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				filenames.push_back(stringFolder + fd.cFileName);
			}
		} while (::FindNextFile(hFind, &fd));
		::FindClose(hFind);
	}
	return (int)filenames.size();
}
#endif


void CardData::MakeHash()
{
	if (myIcon.cols != ImageHash::BITS || myIcon.rows != ImageHash::BITS)
	{
		if (!myInputImage.cols || !myInputImage.rows)
		{
			return;
		}

		float offset = 0.0f;// 
		float part = 1.0f;// 
		cv::Rect subRect(cv::Rect((int)(myInputImage.cols*offset), (int)(myInputImage.cols*offset), (int)(myInputImage.cols*part), (int)(myInputImage.cols*part*0.85f)));
		cv::Mat inputsub(myInputImage, subRect);
		cv::resize(inputsub, myIcon, cv::Size(ImageHash::BITS, ImageHash::BITS), 0, 0, cv::INTER_AREA);
	}

	myImageHash.MakeHash(myIcon);
}

CardData::CardData(const cv::Mat& anInputImage)
	:myInputImage(anInputImage)
{
}

void CardData::Save(cv::FileStorage& fs) const
{
	if (!myImageHash.IsValid())
		return;

	fs << "{";
	fs << "cardId" << myCardId;
	fs << "cardName" << myCardName;
	fs << "imgCoreUrl" << myImgCoreUrl;
	fs << "code" << mySetCode;
	fs << "format" << myFormat;

	std::vector<int> asarray(&myImageHash.myHash32[0], &myImageHash.myHash32[ImageHash::mysize]);
	cv::write(fs, "hash0", asarray);

	fs << "}";
}

void CardData::Load(const cv::FileNode& node)
{
	node["cardId"] >> myCardId;
	node["cardName"] >> myCardName;
	node["imgCoreUrl"] >> myImgCoreUrl;
	node["code"] >> mySetCode;
	node["format"] >> myFormat;

	std::vector<int> asarray;
	const cv::FileNode& h0 = node["hash0"];
	cv::read(h0, asarray);
	for (int i = 0, e = (int)asarray.size(); i < e; ++i)
	{
		myImageHash.myHash32[i] = asarray[i];
	}
}

bool CardData::LoadDisplayImage(const char* directory)
{
	if (!myDisplayImage.empty())
	{
		return true;
	}

	char filename[FILENAME_MAX];
	sprintf(filename, "%s/%s.png", directory, myCardId.c_str());

	myDisplayImage = cv::imread(filename, cv::IMREAD_UNCHANGED);
	if (!myDisplayImage.empty())
	{
		return true;
	}

	return false;
}

void CardData::BuildMatchData()
{
	if (!myIcon.cols || !myIcon.rows)
	{
		if (myDisplayImage.empty())
		{
			return;
		}

		cv::GaussianBlur(myDisplayImage, myDisplayImage, cv::Size(3, 3), 1);
		static cv::Mat resized;
		const int targetSize = 96;
		float scale = float(myDisplayImage.cols) / (float)targetSize;
		cv::resize(myDisplayImage, resized, cv::Size(targetSize, (int)(myDisplayImage.rows / scale)), cv::INTER_AREA);

		ToGray(resized, myInputImage);
	}

	MakeHash();
}

void CardData::setImgCoreUrlFromUri(const char* imgUri)
{
	std::string coreUrl = imgUri;
	size_t start = coreUrl.find("png");
	size_t end = coreUrl.rfind(".png");
	myImgCoreUrl = coreUrl.substr(start + 4, end-(start + 4));
}

void ImageHash::MakeHash(const cv::Mat& iImage)
{
	int temphash[BITS * BITS];
	static cv::Mat subrect;

	for (int y = 0; y < GRID; ++y)
	{
		for (int x = 0; x < GRID; ++x)
		{
			cv::Mat(iImage, cv::Rect(x * CELLBITS, y * CELLBITS, CELLBITS, CELLBITS)).copyTo(subrect);

			int* blocks = &temphash[GetBitIndexFromGridPosition(x, y)];
			for (int i = 0; i < CELLBITS * CELLBITS; i++)
			{
				blocks[i] = subrect.data[i];
			}

			const int m = median(&blocks[0], CELLBITS * CELLBITS);
			for (int i = 0; i < CELLBITS * CELLBITS; i++)
			{
				int v = blocks[i];
				blocks[i] = v > m || (std::abs(v - m) < 1 && m > 128);
			}

		}
	}
	Make32(myHash32, temphash, BITS*BITS);
}
