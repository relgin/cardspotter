#include "CardDatabase.h"
#include "QueryThread.h"
#include <unistd.h>
#include <emscripten.h>
#include <emscripten/fetch.h>

extern "C" {
	CardDatabase gDatabase;
	Query query(gDatabase);

	void PostResults(Result r, bool success, bool autoMatch)
	{
		printf("PostResults %s", (success)?"TRUE":"FALSE");

		const char* cardId = "";
		const char* cardName = "";
		int score = 1024;
		const char* setCode = "";
		char url[256];
		std::vector<cv::Point2f> rectpoints;
		rectpoints.resize(4);
		
		if (success)
		{
			const Match& match = r.myMatch.myList[0];
			sprintf(url, "%s", match.myDatabaseCard->myImgCoreUrl.c_str());

			match.myInput.myRect.points(&rectpoints[0]);
			
			cardId = match.myDatabaseCard->myCardId.c_str();
			cardName = match.myDatabaseCard->myCardName.c_str();
			score = (int)match.myScore[0];
			setCode = match.myDatabaseCard->mySetCode.c_str();
			printf("PostResults success %s", cardId);
		}
	
		EM_ASM_INT({
			if (myActiveTabId<0)
			{
				console.log("No active tab");
				return;
			}
			
			console.log("Variables");
			console.log(AsciiToString($0));
			console.log(AsciiToString($1));
			console.log(AsciiToString($2));
			console.log(AsciiToString($15));
			console.log("Sending");
			chrome.tabs.sendMessage(myActiveTabId,
			{
				cmd: "showresults",
				results : [
					{
						multiverseid : AsciiToString($0),
						name : AsciiToString($1),
						url : AsciiToString($2),
						score : $3,
						px0 : $4,
						py0 : $5,
						px1 : $6,
						py1 : $7,
						px2 : $8,
						py2 : $9,
						px3 : $10,
						py3 : $11,
						pointx : $12,
						pointy : $13,
						isautomatch : ($14==1)?true:false,
						setcode : AsciiToString($15)
					}
				]
			});
			console.log("Sent");

		}, cardId, cardName, url, score,
		(int)rectpoints[0].x, (int)rectpoints[0].y,
		(int)rectpoints[1].x, (int)rectpoints[1].y,
		(int)rectpoints[2].x, (int)rectpoints[2].y,
		(int)rectpoints[3].x, (int)rectpoints[3].y,
		(int)query.myPoint.x, (int)query.myPoint.y,
		(autoMatch)?1:0,
		setCode);
	}
	
	void FindCard(unsigned char* aBuffer, int aBufferLength, int aWidth, int aHeight, int px, int py)
	{
		printf("FindCard %i\n", aBufferLength);
		Result r;
		const bool success = query.FindCardInRoiAndPrint(aBuffer, aBufferLength, aWidth, aHeight, r);
		query.myPoint.x = px - aWidth/2;
		query.myPoint.y = py - aHeight/2;
		
		PostResults(r, success, false);
	}
	
	void AddScreen(unsigned char* aBuffer, int aBufferLength, int aWidth, int aHeight)
	{
		Result r;
		const bool success = query.AddScreenAndPrint(aBuffer, aBufferLength, aWidth, aHeight, r);
		PostResults(r, success, true);
	}

	void SetCardPool(const char* aBuffer, int aBufferLength)
	{
		gDatabase.SetCardPool(aBuffer, aBufferLength);
	}
	
	int SetSetting(const char* aBuffer, int aKeyLength, int aValueLength)
	{
		std::string key(aBuffer, aKeyLength);
		std::string value(&aBuffer[aKeyLength], aValueLength);
//		printf("SetSetting: %s : %s\n", key.c_str(), value.c_str());
		
		const int dbResult = gDatabase.SetSetting(key, value);
		const int queryResult = query.SetSetting(key, value);
		const int result =  std::max(dbResult, queryResult);
//		printf("SetSetting: %i\n", result);
		return result;
	}

	void TestBuffer(unsigned char* aBuffer, int aWidth, int aHeight)
	{
		query.TestBuffer(aBuffer, aWidth, aHeight);
	}
	
	void TestFile(const char * aFile)
	{
		query.TestFile(aFile);
	}
	
	void downloadSucceeded(emscripten_fetch_t *fetch)
	{
	  gDatabase.LoadString(&fetch->data[0]);
	  emscripten_fetch_close(fetch);
	  
	  printf("CardSpotter: Loaded database with %i cards Succeeded\n", gDatabase.myCardCount);
	  EM_ASM(loadingDone(););
	}
	
	void downloadFailed(emscripten_fetch_t *fetch)
	{
	  emscripten_fetch_close(fetch);
	  
	  printf("CardSpotter: download Failed\n");
	  EM_ASM(loadingDone(););
	}

	void LoadDatabase(const char* urlBuffer, int urlLength)
	{
	  std::string url(urlBuffer, urlLength);
	  printf("Loading %s\n", url.c_str());

	  emscripten_fetch_attr_t attr;
	  emscripten_fetch_attr_init(&attr);
	  strcpy(attr.requestMethod, "GET");
	  attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
	  attr.onsuccess = downloadSucceeded;
	  attr.onerror = downloadFailed;
	  emscripten_fetch(&attr, url.c_str());	
	}
}



int main()
{
  EM_ASM(load(););
  return 0;
}

