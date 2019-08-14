#pragma once

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/photo/photo.hpp>
#include <vector>
#include <memory>
#pragma warning(disable:4996)

inline int median(int *data, int n)
{
	int sorted[32 * 32];
	int result;

	memcpy(sorted, data, n * sizeof(int));
	std::nth_element(&sorted[0], &sorted[n / 2], &sorted[n]);
	result = (int)sorted[n / 2];
	if (n % 2 == 1)
	{
		std::nth_element(&sorted[0], &sorted[(n / 2) + 1], &sorted[n]);
		result = (result + sorted[(n / 2) + 1]) / 2;
	}
	return result;
}

inline int NumberOfSetBits(uint32_t i)
{
	i = i - ((i >> 1) & 0x55555555);
	i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
	return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

inline int hammingDistance(const int* hash, const int* hash2, int length)
{
	int distance = 0;
	for (int i = 0; i < length; ++i)
	{
		int xord = hash[i] ^ hash2[i];
		int offbits = NumberOfSetBits(xord);
		distance += offbits;
	}
	return distance;
}

// #pragma optimize("",off)
int getFileNames(const char* folder, std::vector<std::string>& filenames);
struct ImageHash
{
	ImageHash() { memset(myHash32, 0, 32 * sizeof(int32_t)); }
	explicit ImageHash(const cv::Mat& iImage) { memset(myHash32, 0, 32 * sizeof(int32_t)); MakeHash(iImage); }
	bool IsValid() const
	{
		for (int i = 0; i < 32; ++i)
			if (myHash32[i] != 0)
				return true;

		return false;
	}

	int32_t myHash32[32];
	static const int BITS = 32;
	static const int mysize = 32;
	static const int GRID = 4;
	static const int CELLBITS = 32 / GRID;
	static const int CELLINTS = mysize / (GRID*GRID);
	inline static int GetBitIndexFromGridPosition(int x, int y)
	{
		return (y*GRID + x)*CELLBITS*CELLBITS;
	}
	inline static int GetIntIndexFromGridPosition(int x, int y)
	{
		return GetBitIndexFromGridPosition(x, y) / 32;
	}

	inline int GetGridDistance(const ImageHash& anOther, const int x, const int y) const
	{
		return hammingDistance(&myHash32[GetIntIndexFromGridPosition(x, y)], &anOther.myHash32[GetIntIndexFromGridPosition(x, y)], CELLINTS);
	}
	inline int QuickHammingDistance(const ImageHash& anOther) const
	{
		return GetGridDistance(anOther, 1, 1);
	}

	int HammingDistance(const ImageHash& anOther) const
	{
		return hammingDistance(&myHash32[0], &anOther.myHash32[0], mysize);
	}

	void Make32(int* hash32, const int* hash, const int aLength) const
	{
		for (int i = 0; i < aLength; ++i)
		{
			const int bit = i % 32;
			const int index = i / 32;

			int& hashValue = hash32[index];
			if (bit == 0)
			{
				hashValue = 0;
			}
			if (hash[i])
			{
				hashValue |= ((int)1) << bit;
			}
		}
	}

	void MakeHash(const cv::Mat& iImage);
};

struct CardInput;
struct CardData
{
	std::string myCardName;
	std::string myCardId;
	std::string mySetCode;
	std::string myImgCoreUrl;
	int myFormat;
	int mySetIndex;

	cv::Mat myDisplayImage;
	cv::Mat myInputImage;
	cv::Mat myIcon;

private:
	ImageHash myImageHash;
public:
	void MakeHash();
	const ImageHash& GetHash() const { return myImageHash; }

	CardData() {}
	CardData(const cv::Mat& anInputImage);

	enum Type
	{
		OLD = 1 << 0,
		BASIC = 1 << 1,
		NEW = 1 << 2,
		ALL = OLD | BASIC | NEW
	};

	void Save(cv::FileStorage& fs) const;

	void Load(const cv::FileNode& node);

	bool LoadDisplayImage(const char* directory);

	void BuildMatchData();

	static bool ToGray(const cv::Mat& anInput, cv::Mat& output)
	{
		if (anInput.channels() == 3)
		{
			cv::cvtColor(anInput, output, cv::COLOR_BGR2GRAY);
			return true;
		}

		cv::cvtColor(anInput, output, cv::COLOR_BGRA2GRAY);
		return true;
	}

	void setImgCoreUrlFromUri(const char* imgUri);
};

struct CardInput
{
	CardInput() :myDebugTagged(false) {}
	CardInput(const cv::RotatedRect& rect, const cv::Mat& aMat, const char* aPath)
		:myDebugTagged(false)
	{
		myRect = rect;
#if _WIN32
		if (aMat.rows < aMat.cols)
		{
			int * crash = NULL;
			*crash = 1;
		}
#endif
		myQuery.myInputImage = aMat;
		myDebugPath += aPath;
	}

	CardData myQuery;
	std::string myDebugPath;
	cv::RotatedRect myRect;
	bool myDebugTagged;
};

static void GrabFullCardFlips(const cv::Mat& aCard, const cv::RotatedRect& baseRect, std::vector<CardInput>& oList, bool shouldFlip, const char* comment)
{
	cv::Mat card;
	if (aCard.rows < aCard.cols)
	{
		cv::transpose(aCard, card);
		cv::flip(card, card, 0);
	}
	else
	{
		card = aCard.clone();
	}

	CardInput cInput(baseRect, card.clone(), comment);
	oList.push_back(cInput);
	if (shouldFlip)
	{
		cv::flip(card, card, -1);
		CardInput cInputF(baseRect, card, comment);
		oList.push_back(cInputF);
	}
}

struct Match
{
	Match() :myDatabaseCard(0) { memset(myScore, 0, sizeof(float)*SCORES); }
	Match(const CardData* data) :myDatabaseCard(data)
	{
		memset(myScore, 0, sizeof(float)*SCORES);
	}

	const CardData* myDatabaseCard;
	int myIteration;
	int myPotentialRectIndex;
	const static int SCORES = 2;
	float myScore[SCORES];
	int myVariant;
	CardInput myInput;
};
