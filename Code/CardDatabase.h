#pragma once

#include "CardData.h"
// #ifdef USE_LZMA
// #include <LzmaLib.h>
// #endif

#include <iostream>
#include <fstream>
#include <functional>
#include <map>

struct CardList
{
	CardList() { myCardData.reserve(100); myHashes.reserve(100); }
	void AddCardData(CardData* aData)
	{
		myCardData.push_back(aData);
		myHashes.push_back(aData->GetHash());
	}
	void Clear();
	std::vector<CardData*> myCardData;
	std::vector<ImageHash> myHashes;
};

struct CardDatabase
{
	CardDatabase();

	std::map<std::string, std::vector<CardData>> myCardsbyName;
	std::vector<std::string> mySetNames;
	std::vector<std::vector<CardData>> mySetCards;

	std::vector<std::string> myCustomcardpool;

	int SetSetting(const std::string& key, const std::string& value);

	void SetCardPool(const char* aBuffer, int aBufferLength);

	std::vector<const CardList*> myCardLists;
	CardList myCardPool;
	CardList myBasics;
	CardList myNewSets;
	CardList myOldSets;

	int myCardCount;
	int myCurrentFormat;
	CardData* GetDataById(const char* aCardId);

	void LoadString(const char* fsData)
	{
		cv::FileStorage fs(fsData, cv::FileStorage::READ | cv::FileStorage::MEMORY);
		LoadFromFileStorage(fs);
	}
	
	void LoadFromTextFile(const char* aFileName)
	{
		cv::FileStorage fs(aFileName, cv::FileStorage::READ);
		LoadFromFileStorage(fs);
	}

	void LoadFromFileStorage(cv::FileStorage& fs);
	void BuildCardLists();

	void SaveAsTextFile(const char* aFilename, bool allowEmptyImage = false);

	typedef std::function<bool (const char* aSet, CardData& cardData, cv::Mat&)> ProcessCardFunction;

	void BuildSetCards();
	void BuildDatabaseFromDisplayImages(ProcessCardFunction processCardFunction);

	void GetCardsForCurrentFormat(CardList& outSet);
	void GetCardsByName(const std::string& aName, CardList& outSet);
	static std::string GetHashname(const std::string& aName);
	void Optimize();
	void KeepOnlyFormat(int keepMask);
};
