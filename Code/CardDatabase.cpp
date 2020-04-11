#include "CardDatabase.h"

// #pragma optimize("",off)

CardDatabase::CardDatabase()
	:myCurrentFormat(0)
	,myCardCount(0)
{
}

int CardDatabase::SetSetting(const std::string& key, const std::string& value)
{
	if (strcmp(key.c_str(), "cardpool") == 0)
	{
		SetCardPool(value.c_str(), value.size());
		return myCustomcardpool.size();
	}
	return -1;
}

void CardDatabase::SetCardPool(const char* aBuffer, int aBufferLength)
{
	myCustomcardpool.clear();
	std::stringstream ss;
	ss.write(aBuffer, aBufferLength);
	std::string to;
	while (std::getline(ss, to)) {
		myCustomcardpool.push_back(to);
	}
	BuildCardLists();
}

CardData* CardDatabase::GetDataById(const char* aCardId)
{
	for (auto& cardArray : mySetCards)
	{
		for (CardData& data : cardArray)
		{
			if (data.myCardId == aCardId)
			{
				return &data;
			}
		}
	}
	return nullptr;
}

void CardDatabase::LoadFromFileStorage(cv::FileStorage& fs)
{
	myCardCount = 0;
	cv::FileNode setNamesNode = fs["setNames"];
	cv::FileNodeIterator namesIT = setNamesNode.begin(), namesIT_end = setNamesNode.end(); // Go through the node
	for (; namesIT != namesIT_end; ++namesIT)
	{
		std::string name = *namesIT;
		mySetNames.push_back(name);
	}
	cv::FileNode setCardsNode = fs["setCards"];
	cv::FileNodeIterator setCardsIT = setCardsNode.begin(), setCardsIT_end = setCardsNode.end(); // Go through the node
	for (; setCardsIT != setCardsIT_end; ++setCardsIT)
	{
		std::vector<CardData> setCards;
		cv::FileNode cardsNode = (*setCardsIT)["cards"];
		cv::FileNodeIterator cardsIT = cardsNode.begin(), cardsIT_end = cardsNode.end(); // Go through the node
		for (; cardsIT != cardsIT_end; ++cardsIT)
		{
			CardData data;
			data.Load(*cardsIT);
			++myCardCount;
			myCardsbyName[GetHashname(data.myCardName)].push_back(data);
			setCards.push_back(data);
		}
		mySetCards.push_back(setCards);
	}

	fs.release();

	BuildCardLists();
}

void CardDatabase::BuildCardLists()
{
	myCardLists.clear();

	if (myCustomcardpool.size())
	{
		myCardPool.Clear();
		for (const auto& name : myCustomcardpool)
		{
			for (auto& cardData : myCardsbyName[GetHashname(name)])
			{
				myCardPool.AddCardData(&cardData);
			}
		}
		if (myCardPool.myCardData.size())
		{
			myCardLists.push_back(&myCardPool);
		}
	}
	
	if (myCardLists.size()==0)
	{
		int currentFormat = myCurrentFormat;
		myBasics.Clear();
		myCurrentFormat = CardData::BASIC;
		GetCardsForCurrentFormat(myBasics);
		myCardLists.push_back(&myBasics);

		myCurrentFormat = CardData::NEW;
		GetCardsForCurrentFormat(myNewSets);
		myCardLists.push_back(&myNewSets);

		myCurrentFormat = CardData::OLD;
		GetCardsForCurrentFormat(myOldSets);
		myCardLists.push_back(&myOldSets);

		myCurrentFormat = currentFormat;
	}
}

void CardDatabase::SaveAsTextFile(const char* aFilename, bool allowEmptyImage /*= false*/)
{
	BuildSetCards();

	cv::FileStorage fs(aFilename, cv::FileStorage::WRITE);
	fs << "setNames" << "[";
	for (const std::string& setName : mySetNames)
	{
		fs << setName;
	}
	fs << "]";

	fs << "setCards" << "[";
	for (const auto& cardArrays : mySetCards)
	{
		fs << "{" << "cards" << "[";
		for (const CardData& cardData : cardArrays)
		{
			cardData.Save(fs);
		}
		fs << "]" << "}";
	}
	fs << "]";
}

void CardDatabase::BuildSetCards()
{
	mySetCards.clear();
	mySetCards.resize(mySetNames.size());
	for (auto& cardArray : myCardsbyName)
	{
		for (CardData& cardData : cardArray.second)
		{
			std::vector<CardData>& setCards = mySetCards[cardData.mySetIndex];
			setCards.push_back(cardData);
		}
	}
}

void CardDatabase::BuildDatabaseFromDisplayImages(ProcessCardFunction processCardFunction)
{
	int i = 0;
	int percent = 0;
	for (auto& cardArray : myCardsbyName)
	{
		int newPercent = (i * 100) / myCardsbyName.size();
		if (newPercent>percent)
		{
			percent = newPercent;
			std::cout << percent << "%" << std::endl;
		}
		++i;
		for (CardData& cardData : cardArray.second)
		{
			processCardFunction("images", cardData, cardData.myDisplayImage);
		}
	}
	BuildCardLists();
}

void CardDatabase::GetCardsForCurrentFormat(CardList& outSet)
{
	outSet.Clear();
	for (auto& cardArray : mySetCards)
	{
		for (CardData& cardData : cardArray)
		{
			bool rightFormat = myCurrentFormat == 0 || (cardData.myFormat & myCurrentFormat);
			if (rightFormat)
			{
				outSet.AddCardData(&cardData);
			}
		}
	}
}

void CardDatabase::GetCardsByName(const std::string& aName, CardList& outSet)
{
	for (auto& cardData : myCardsbyName[GetHashname(aName)])
	{
		outSet.AddCardData(&cardData);
	}
}

std::string CardDatabase::GetHashname(const std::string& aName)
{
	std::string hashname = aName;
	hashname.erase(remove_if(hashname.begin(), hashname.end(),
		[](char c) { return isalnum(c) == 0; }), hashname.end());
	std::transform(hashname.begin(), hashname.end(), hashname.begin(), ::tolower);
	return hashname;
}

void CardDatabase::Optimize()
{
	for (auto& cardArray : myCardsbyName)
	{
		std::vector<CardData>& arts = cardArray.second;
		std::reverse(arts.begin(), arts.end());
		for (int i = 0; i < arts.size(); ++i)
		{
			const ImageHash& hash = arts[i].GetHash();
			for (int j = (int)arts.size()-1; j > i; --j)
			{
				CardData& otherCard = arts[j];
				int hamming = hash.HammingDistance(otherCard.GetHash());
				if (hamming < 100)
				{
					std::vector<CardData>& setCards = mySetCards[otherCard.mySetIndex];

					for (int s = 0, se = (int)setCards.size(); s < se; ++s)
					{
						if (GetHashname(otherCard.myCardName) == GetHashname(setCards[s].myCardName))
						{
							setCards.erase(setCards.begin() + s);
							break;
						}
					}

					arts.erase(arts.begin() + j);
				}
			}
		}
	}
}

void CardDatabase::KeepOnlyFormat(int keepMask)
{
	auto it = myCardsbyName.begin();
	while (it != myCardsbyName.end())
	{
		if (!it->second.size() || !(it->second[0].myFormat & keepMask))
		{
			it = myCardsbyName.erase(it);
		}
		else
		{
			it++;
		}
	}
}

void CardList::Clear()
{
	myCardData.clear();
	myHashes.clear();
}
