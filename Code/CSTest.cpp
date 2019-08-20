// CSTest.cpp : Defines the entry point for the console application.
//

#include "QueryThread.h"
#include "CardDatabase.h"
#include <windows.h>

//#pragma optimize("",off)
const char* cardPool = "bloodbraid elf";
CardDatabase gDatabase;
Query query(gDatabase);

int main()
{
	gDatabase.LoadFromTextFile("magic.db");
	cv::setUseOptimized(false);
	cv::setNumThreads(1);
	query.myOkMatchScore = 280;
	static bool videoTest = true;
	static bool clickTest = false;
	static bool singleTest = false;
	if (singleTest)
	{
		query.myMinCardHeightRelative = 0.5f;
		query.myMaxCardHeightRelative = 0.7f;
		query.TestFile("Regression/priest of titania.png");
	}
	const int before = TimeNow();

// 	gDatabase.SetCardPool(cardPool, strlen(cardPool));

	int testTime = 0;
	int succeeded = 0;
	int tests = 0;
	Result result;
	std::vector<std::string> filenames;
	if (videoTest)
	{
		query.myMinCardHeightRelative = 0.5f;
		query.myMaxCardHeightRelative = 0.7f;

		getFileNames("Regression/Auto1", filenames);

		for (const std::string& file : filenames)
		{
			cv::Mat* input;
			cv::Mat screen = cv::imread(file);
			input = &screen;

			cv::Mat resized;
			if (screen.rows != 720)
			{
				const float scale = float(screen.rows) / 720.f;
				cv::resize(screen, resized, cv::Size(int(float(screen.cols) / scale), 720));
				input = &resized;
			}

			std::cout << "|";
			if (query.AddScreenBGR(*input, testTime, result))
			{
				const Match& match = result.myMatch.myList[0];
				std::cout << std::endl << file << " [" << match.myScore[0] << ", " << match.myScore[1] << "][" << match.myDatabaseCard->mySetCode << "]: " << match.myDatabaseCard->myCardName << std::endl;
// 				cv::Mat inputImage = result.myMatch.myCard.mypotenatialRects[match.mypotentialRectIndex].myvariations[match.variant].myquery.myInputImage;
// 				cv::imwrite("Regression/DebugOutput/debugImage.png", inputImage);

				++succeeded;
			}
			testTime += 250;//simulate 4fps
			++tests;
// 			if (tests>32)
// 				break;
		}
	}

	if (clickTest)
	{
		getFileNames("Regression", filenames);

		for (const std::string& file : filenames)
		{
			cv::Mat screen = cv::imread(file);
			++tests;
			if (query.TestFile(file, false))
			{
				++succeeded;
			}
		}
	}

	std::cout << succeeded << "/" << tests << " in " << (TimeNow() - before) << "ms ";

	std::ostringstream os_;
	os_ << succeeded << "/" << tests << " in " << (TimeNow() - before) << "ms ";
	OutputDebugString(os_.str().c_str());

	int x;
	std::cin >> x;
	return 0;
}

