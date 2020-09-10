#pragma once

#include <iostream>     // std::cout
#include <fstream>      // std::ifstream
#include <sstream>
#include <vector>
#include <curl/curl.h>
#include <Winhttp.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include "CardData.h"

extern const char* appData;
#pragma optimize("",off)


static inline bool fileExists(const std::string& name) {
	struct stat buffer;
	return (stat(name.c_str(), &buffer) == 0);
}

static size_t curlWriteFile(void *ptr, size_t size, size_t nmemb, FILE *stream) {
	size_t written = fwrite(ptr, size, nmemb, stream);
	return written;
}


static void SetCurlProxy(CURL * curl)
{
	WINHTTP_CURRENT_USER_IE_PROXY_CONFIG proxy;
	if (WinHttpGetIEProxyConfigForCurrentUser(&proxy))
	{
		if (proxy.lpszProxy)
		{
			curl_easy_setopt(curl, CURLOPT_PROXY, proxy.lpszProxy);
		}
	}
}

static bool CurlUrlToFile(const char * outfilename, const char * url, int aTimeout = 0)
{
	FILE *fp;
	CURLcode res;

	CURL *curl = curl_easy_init();
	if (!curl)
	{
		return false;
	}

	fp = fopen(outfilename, "wb");
	if (!fp)
	{
		return false;
	}
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/85.0.4183.83 Safari/537.36");
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, aTimeout);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteFile);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, true);
	// 		SetCurlProxy(curl);
	res = curl_easy_perform(curl);
	/* always cleanup */
	curl_easy_cleanup(curl);
	fclose(fp);

	Sleep(1500);

	return res == CURLcode::CURLE_OK;
}

static std::ifstream::pos_type filesize(const char* filename)
{
	std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
	return in.tellg();
}
static bool IsValidCachedImageFile(const char* aFilename, int sizeRequirement = 15000)
{
	return fileExists(aFilename) && filesize(aFilename) > sizeRequirement;
}

static bool GetCutImageById(const char* aSet, const CardData& cardData, cv::Mat& outImage)
{
	char outfilename[FILENAME_MAX];
	{
		sprintf(outfilename, "magic/%s/cut/%s.png", aSet, cardData.myCardId.c_str());
	}

	if (IsValidCachedImageFile(outfilename))
	{
		outImage = cv::imread(outfilename, cv::IMREAD_UNCHANGED);
		if (outImage.data)
		{
			return true;
		}
	}

	return false;
}


static bool GetGathererImageById(const char* aSet, const CardData& cardData, cv::Mat& outImage)
{
	//NORMAL PNG FIRST
// 	static char* gurl = "http://www.cardspotter.com/magic/images/%s.png";
	static char* gurl = "http://gatherer.wizards.com/Handlers/Image.ashx?multiverseid=%s&type=card";
	char outfilename[FILENAME_MAX];
	char url[512];
	sprintf(outfilename, "magic/%s/gatherer/%s.png", aSet, cardData.myCardId.c_str());

	if (IsValidCachedImageFile(outfilename))
	{
		outImage = cv::imread(outfilename, cv::IMREAD_UNCHANGED);
		if (outImage.data)
		{
			return true;// IsValidImageRatio(outImage.cols, outImage.rows);
		}
	}

	std::remove(outfilename);
	sprintf(url, gurl, cardData.myCardId);
	if (CurlUrlToFile(outfilename, url, 3))
	{
		outImage = cv::imread(outfilename, cv::IMREAD_UNCHANGED);
		if (outImage.data)
		{
			return true;//IsValidImageRatio(outImage.cols, outImage.rows);
		}
	}

	return false;
}

static bool CacheImageById(const char* anImageId)
{
	static char* cardSpotter = "http://www.cardspotter.com/magic/images/%s.png";
// 	static char* zamImg = "http://wow.zamimg.com/images/hearthstone/cards/enus/original/%s.png?12577";
	// 	static char* baseLocation = zamImg;
	static bool allowAltLocation = false;
	char outfilename[FILENAME_MAX];
	char url[512];

	//NORMAL PNG FIRST
	sprintf(outfilename, "%s/CardSpotter/Cache/%s.png", appData, anImageId);

	if (!IsValidCachedImageFile(outfilename))
	{
		std::remove(outfilename);
		sprintf(url, cardSpotter, anImageId);
		if (CurlUrlToFile(outfilename, url, 3))
		{
			return true;
		}

// 		if (allowAltLocation)
// 		{
// 			sprintf(url, zamImg, anImageId);
// 			if (CurlUrlToFile(outfilename, url, 3))
// 			{
// 				return true;
// 			}
// 		}
		return false;
	}

	//ANIMATED AFTER
// 	sprintf(outfilename, "%s/CardSpotter/Cache/%s.gif", appData, anImageId);
// 
// 	if (!fileExists(outfilename))
// 	{
// 		sprintf(url, "http://wow.zamimg.com/images/hearthstone/cards/enus/animated/%s_premium.gif", anImageId);
// 		if (!CurlUrlToFile(outfilename, url))
// 		{
// 			return false;
// 		}
// 	}
	return true;
}

static size_t curl_OStreamWrite(void* buf, size_t size, size_t nmemb, void* userp)
{
	if (userp)
	{
		std::ostream& os = *static_cast<std::ostream*>(userp);
		std::streamsize len = size * nmemb;
		if (os.write(static_cast<char*>(buf), len))
			return len;
	}

	return 0;
}
