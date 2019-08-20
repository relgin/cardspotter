// HSDB.cpp : Defines the entry point for the console application.
//

#include "rapidjson/document.h"
#include <iostream>     // std::cout
#include <fstream>      // std::ifstream
#include <vector>
#include <curl/curl.h>
#include <opencv2/core/types_c.h>
#include <string>
// #pragma optimize("", off)

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#include <Windows.h>
#include "CardDatabase.h"
#include "CardData.h"
#include "CardCurl.h"
// #include "grfmt_png.hpp"
#include "opencv2/core/core.hpp"
#include "QueryThread.h"
#include <direct.h> //mkdir n such

static const char* allowHttpImageGetSet = "";

inline bool Deviates(cv::Vec3b value, int anotherValue, int thresh)
{
	for (int i = 0; i<3; ++i)
	{
		int valueX = value(i);
		if (std::abs(valueX - anotherValue) > thresh)
		{
			return true;
		}
	}
	return false;
}

inline bool HasColor(const cv::Mat& aMat, int aStartX, int anEndX, int aStartY, int anEndY, std::vector<int>& row)
{
	int totalPixels = ((anEndX - aStartX) + 1) * ((anEndY - aStartY) + 1);
	int color = 0;

	bool firstRow = row[0] == INT_MAX;
	int currentPixel = 0;
	int previousAvg = 0;
	for (int x = aStartX; x <= anEndX; ++x)
	{
		for (int y = aStartY; y <= anEndY; ++y)
		{
			cv::Vec3b current = aMat.at<cv::Vec3b>(y, x);
			int avgValue = (current(0) + current(1) + current(2)) / 3;
			if (currentPixel > 0)
			{
				if (cv::norm(avgValue - previousAvg) > 28)
				{
					return true;
				}
			}
			previousAvg = avgValue;

			int previousRowAvg = row[currentPixel];
			row[currentPixel] = avgValue;
			currentPixel++;

			if (avgValue < 15)
			{
				continue;
			}

			if (!Deviates(current, avgValue, 3))
			{
				continue;
			}

			if (avgValue > 235)
			{
				continue;
			}

			if (Deviates(current, avgValue, 18))
			{
				return true;
			}

			if (!firstRow)
			{
				if (cv::norm(previousRowAvg - avgValue) > 28)
				{
					return true;
				}
			}
		}
	}
	return false;
}

cv::Rect ShrinkWrap(const cv::Mat &image)
{
	const int searchHalfWidth = image.cols / 4;
	std::vector<int> row;
	row.resize(searchHalfWidth * 2 + 1);
	row[0] = INT_MAX;
	int bottomY = image.rows - 3;
	for (; !HasColor(image, image.cols / 2 - searchHalfWidth, image.cols / 2 + searchHalfWidth, bottomY, bottomY, row); --bottomY);

	int topY = 3;
	row[0] = INT_MAX;
	for (; !HasColor(image, image.cols / 2 - searchHalfWidth, image.cols / 2 + searchHalfWidth, topY, topY, row); ++topY);

	int leftX = 3;
	row[0] = INT_MAX;
	for (; !HasColor(image, leftX, leftX, image.rows / 2 - searchHalfWidth, image.rows / 2 + searchHalfWidth, row); ++leftX);

	int rightX = image.cols - 3;
	row[0] = INT_MAX;
	for (; !HasColor(image, rightX, rightX, image.rows / 2 - searchHalfWidth, image.rows / 2 + searchHalfWidth, row); --rightX);

	leftX++;
	topY++;
	rightX--;
	bottomY--;
	cv::Rect rect(leftX, topY, (rightX - leftX), (bottomY - topY));
	return rect;
}

bool AutoRecut(const std::string& file, cv::Mat& outMat)
{
	cv::Mat image = cv::imread(file, cv::IMREAD_COLOR);
	if (image.cols < 100 || image.rows < 100)
	{
		return false;
	}
	cv::Rect rect = ShrinkWrap(image);

	cv::Mat subImage(image, rect);
	if (subImage.cols < 100 || subImage.rows < 100)
	{
		return false;
	}

	float widthToHeightScale = static_cast<float>(subImage.rows) / static_cast<float>(subImage.cols);
	cv::resize(subImage, outMat, cv::Size(256, (int)(ceil(256 * widthToHeightScale))), cv::INTER_LANCZOS4);
	return true;
}

void AddCardToDatabase(CardData &card, CardDatabase &setDatabase)
{
	++setDatabase.myCardCount;
	card.mySetIndex = setDatabase.mySetNames.size() - 1;
	std::string hashName = CardDatabase::GetHashname(card.myCardName);
	std::vector<CardData>& list = setDatabase.myCardsbyName[hashName];

	for (CardData& existingCard : list)
	{
		card.myFormat |= existingCard.myFormat;
		existingCard.myFormat = card.myFormat;
	}
	list.push_back(card);
}

rapidjson::Document ReadJson(const char * jsonfile)
{
	rapidjson::Document d;
	std::ifstream ifs(jsonfile);
	ifs.seekg(0, ifs.end);
	int length = (int)ifs.tellg();
	if (length > 0)
	{
		ifs.seekg(0, ifs.beg);

		char * buffer = new char[length];;
		ifs.read(buffer, length);
		d.Parse<rapidjson::kParseStopWhenDoneFlag>(buffer);
	}
	else
	{
		std::cout << " failed to read " << jsonfile << std::endl;
	}
	return d;
}

static bool DownloadScryfallPng(const CardData &cardData, char * fileName)
{
	if (IsValidCachedImageFile(fileName))
		return true;

	std::remove(fileName);
	std::string url("https://img.scryfall.com/cards/png/");
	url += cardData.myImgCoreUrl;
	url += ".png";
	Sleep(3000);
	if (CurlUrlToFile(fileName, url.c_str(), 10))
	{
		std::cout << "CURL :[" << cardData.mySetCode << "," << cardData.myCardId << "]" << cardData.myCardName << std::endl;
		return true;
	}
	else
	{
		std::remove(fileName);
		std::cout << "NO CURL :[" << cardData.mySetCode << "," << cardData.myCardId << "]" << cardData.myCardName << std::endl;
		return false;
	}
}

static void WriteCompressedPng(const cv::Mat& image, const char * fileName)
{
	std::vector<int> compression_params;
	compression_params.push_back(cv::IMWRITE_PNG_COMPRESSION);
	compression_params.push_back(9);
	cv::imwrite(fileName, image, compression_params);
}

static bool CutImage(const CardData &cardData, char * cutFilename)
{
	if (strlen(allowHttpImageGetSet) > 0 && stricmp(allowHttpImageGetSet, cardData.mySetCode.c_str()) != 0)
	{
		return false;
	}

	char rawFilename[FILENAME_MAX];
	sprintf(rawFilename, "scryfall/s%s/%s.png", cardData.mySetCode.c_str(), cardData.myCardId.c_str());
	if (!DownloadScryfallPng(cardData, rawFilename))
	{
		return false;
	}

	cv::Mat resized;
	if (!AutoRecut(rawFilename, resized))
	{
		std::cout << "NO CUT :[" << cardData.mySetCode << "," << cardData.myCardId << "]" << cardData.myCardName << std::endl;
		return false;
	}
	WriteCompressedPng(resized, cutFilename);
	return true;
}

static bool GetScryfallByCard(const char* /*aSet*/, CardData& cardData, cv::Mat& outImage)
{
	char iconFilename[FILENAME_MAX];
	sprintf(iconFilename, "scryfall/icon/s%s/%s.png", cardData.mySetCode.c_str(), cardData.myCardId.c_str());

	cv::Mat icon = cv::imread(iconFilename, cv::IMREAD_UNCHANGED);
	if (icon.cols == ImageHash::BITS && icon.rows == ImageHash::BITS)
	{
		cardData.myIcon = icon;
		cardData.BuildMatchData();
		cardData.myDisplayImage = cv::Mat();
		cardData.myInputImage = cv::Mat();
		return true;
	}

	char cutFilename[FILENAME_MAX];
	sprintf(cutFilename, "scryfall/cut/s%s/%s.png", cardData.mySetCode.c_str(), cardData.myCardId.c_str());

	if (!IsValidCachedImageFile(cutFilename) && !CutImage(cardData, cutFilename))
	{
		return false;
	}

	outImage = cv::imread(cutFilename, cv::IMREAD_UNCHANGED);
	if (outImage.data && (fabs(((float)outImage.cols / (float)outImage.rows) - 0.7f) < 0.1f))
	{
		cardData.BuildMatchData();

		char iconFilename[FILENAME_MAX];
		sprintf(iconFilename, "scryfall/icon/s%s/%s.png", cardData.mySetCode.c_str(), cardData.myCardId.c_str());
		if (!IsValidCachedImageFile(iconFilename, 500))
		{
			cv::imwrite(iconFilename, cardData.myIcon);
		}

		cardData.myDisplayImage = cv::Mat();
		cardData.myInputImage = cv::Mat();

		return true;
	}
	std::cout << "ERROR :[" << cardData.mySetCode << "," << cardData.myCardId << "]" << cardData.myCardName << std::endl;
	return false;
}

void ScryfallBuild()
{
	CardDatabase database;

	const char* scryfallSets = "scryfall/scryfallsets.json";
	std::remove(scryfallSets);
	if (!fileExists(scryfallSets) && filesize(scryfallSets) < 100 && !CurlUrlToFile(scryfallSets, "https://api.scryfall.com/sets", 10))
	{
		return;
	}

	rapidjson::Document d = ReadJson(scryfallSets);
	const auto& setList = d["data"];

	char jsonfile[FILENAME_MAX];
	char url[FILENAME_MAX];
	char dir[FILENAME_MAX];

	for (auto setIT = setList.Begin(); setIT != setList.End(); ++setIT)
	{
		const auto& set = *setIT;
		const char* set_type = set["set_type"].GetString();
		if (stricmp(set_type, "funny") == 0)
		{
			continue; //Crashes on some unglued name I think
		}
		const char* code = set["code"].GetString();
		if (strncmp(code, "pmps", 4) == 0 || strncmp(code, "pcel", 4) == 0)
		{
			continue; 
		}
		database.mySetNames.push_back(code);
		database.mySetCards.push_back(std::vector<CardData>());


		const char* parent_set_code = set["parent_set_code"].GetString();
		const char* released_at = 0;
		if (parent_set_code)
		{
			for (auto pSetIT = setList.Begin(); pSetIT != setList.End(); ++pSetIT)
			{
				const auto& pset = *pSetIT;
				const char* pcode = pset["code"].GetString();
				if (stricmp(pcode, parent_set_code) == 0)
				{
					released_at = pset["released_at"].GetString();
					break;
				}
			}
		}
		else
		{
			released_at = set["released_at"].GetString();
		}
		int setformat = CardData::OLD;
		if (released_at)
		{
			int year = atoi(released_at);
			if (year >= 2018)
			{
				setformat = CardData::NEW;
			}
		}

		_mkdir("scryfall");
		sprintf(dir, "scryfall/s%s", code);
		_mkdir(dir);

		_mkdir("scryfall/cut");
		sprintf(dir, "scryfall/cut/s%s", code);
		_mkdir(dir);

		_mkdir("scryfall/icon");
		sprintf(dir, "scryfall/icon/s%s", code);
		_mkdir(dir);

		int page = 1;
		while (page > 0)
		{
			sprintf(url, "https://api.scryfall.com/cards/search?order=set&q=%%2B%%2Be%%3A%s&page=%i", code, page);
			sprintf(jsonfile, "scryfall/%s-%i.json", code, page);
			std::cout << code << page;
			rapidjson::Document set;

			const bool exists = fileExists(jsonfile) && filesize(jsonfile) > 100;
			if (exists)
			{
				set = ReadJson(jsonfile);
			}

			if (!exists || set.HasParseError())
			{
				Sleep(100);
				CurlUrlToFile(jsonfile, url, 10);
				set = ReadJson(jsonfile);
			}

			if (!set.HasParseError())
			{
				std::cout << " Success";

				const auto& cardList = set["data"];
				for (auto cardIT = cardList.Begin(); cardIT != cardList.End(); ++cardIT)
				{
					const auto& card = *cardIT;
					const char* name = card["name"].GetString();
					if (stricmp(code, "puma") == 0 && stricmp(name, "platinum emperion") == 0)
					{
						continue;
					}
					if (stricmp(code, "puma") == 0 && stricmp(name, "vengevine") == 0)
					{
						continue;
					}

					const char* layout = card["layout"].GetString();
					if (stricmp(layout, "scheme") == 0)
					{
						continue;
					}

					const char* lang = card["lang"].GetString();
					if (stricmp(lang, "en") != 0)
					{
						continue;
					}

					const char* border_color = card["border_color"].GetString();
					if (stricmp(border_color, "black") != 0)
					{
						continue;
					}
					
					int oldNewBasic = setformat;

					if (stricmp(layout, "normal") == 0)
					{
						std::string type = card["type_line"].GetString();
						if (type.find("Basic Land") != -1)
						{
							oldNewBasic = CardData::BASIC;
						}
					}
					else if (stricmp(layout, "vanguard") == 0)
					{
						continue;
					}
					


					if (card["image_uris"].Size())
					{
						CardData cData;
						cData.myCardId = card["collector_number"].GetString();
						cData.myCardName = name;
						cData.mySetCode = code;
						cData.myFormat = oldNewBasic;
						cData.setImgCoreUrlFromUri(card["image_uris"]["png"].GetString());
						AddCardToDatabase(cData, database);
					}
					else
					{
						const auto& card_faces = card["card_faces"];
						if (card_faces.Size())
						{
							CardData sideA;
							sideA.myCardId = card["collector_number"].GetString();
							sideA.myCardName = name;
							sideA.mySetCode = code;
							sideA.myFormat = oldNewBasic;
							CardData sideB = sideA;
							if (card_faces[0]["image_uris"].Size())
							{
								sideA.myCardId += "a";
								sideA.setImgCoreUrlFromUri(card_faces[0]["image_uris"]["png"].GetString());
								AddCardToDatabase(sideA, database);
							}

							if (stricmp(layout, "transform") == 0)
							{
								if (card_faces[1]["image_uris"].Size())
								{
									sideB.myCardId += "b";
									sideB.setImgCoreUrlFromUri(card_faces[1]["image_uris"]["png"].GetString());
									AddCardToDatabase(sideB, database);
								}
							}
						}
					}
				}

				if (set["has_more"].GetBool())
				{
					page++;
				}
				else
				{
					page = -1;
				}
			}
			else
			{
				page = -1;
				std::cout << " Failed: " << set.GetParseError();
				std::remove(jsonfile);
			}
			std::cout << std::endl;
		}
	}

	database.BuildDatabaseFromDisplayImages(GetScryfallByCard);

	database.Optimize();
	database.SaveAsTextFile("magic.db");
}

static bool AutoCutDisplayImage(const char* /*aSet*/, CardData& cardData, cv::Mat& outImage)
{
	char fileName[FILENAME_MAX];
	sprintf(fileName, "%s/%s.png", cardData.mySetCode.c_str(), cardData.myCardId.c_str());

	if (!AutoRecut(fileName, outImage))
	{
		std::cout << "NO CUT :[" << cardData.mySetCode << "," << cardData.myCardId << "]" << cardData.myCardName << std::endl;
		return false;
	}

	if (outImage.data && (fabs(((float)outImage.cols / (float)outImage.rows) - 0.7f) < 0.1f))
	{
		return true;
	}
	std::cout << "ERROR :[" << cardData.mySetCode << "," << cardData.myCardId << "]" << cardData.myCardName << std::endl;
	return false;
}

void FolderBuild(const char* folder)
{
	CardDatabase database;
	database.mySetNames.push_back(folder);
	std::vector<std::string> filenames;
	getFileNames(folder, filenames);

	for (const std::string& file : filenames)
	{
		int lastSlash = file.find_last_of('/');
		int suffixStart = file.find_last_of('.');

		CardData cData;
		cData.myCardId = file.substr(lastSlash+1, suffixStart-(lastSlash+1));
		cData.myCardName = cData.myCardId;
		cData.mySetCode = folder;
		cData.myFormat = CardData::NEW;
		cData.myImgCoreUrl = cData.myCardId;
		AddCardToDatabase(cData, database);
	}

	database.BuildDatabaseFromDisplayImages(AutoCutDisplayImage);

	database.Optimize();
	char dbFile[FILENAME_MAX];
	sprintf(dbFile, "%s.db", folder);
	database.SaveAsTextFile(dbFile);
}


int main(int argc, char* argv[])
{
	ScryfallBuild();
// 	FolderBuild("images");
	return 0;
} 

