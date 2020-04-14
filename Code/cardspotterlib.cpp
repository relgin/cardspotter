#include "CardDatabase.h"
#include "QueryThread.h"
#include <unistd.h>
#include <emscripten.h>
#include <emscripten/bind.h>

using namespace emscripten;
using namespace std;

class CardSpotter
{
private:
    CardDatabase gDatabase;
    Query query;

public:
    CardSpotter() : query(gDatabase)
    {
    }

    int SetSetting(std::string key, std::string value)
    {
        const int dbResult = gDatabase.SetSetting(key, value);
        const int queryResult = query.SetSetting(key, value);
        const int result = std::max(dbResult, queryResult);
//        printf("SetSetting %s, %s, %i\n", key.c_str(), value.c_str(), result);
        return result;
    }

    int LoadDatabase(std::string data)
    {
        gDatabase.LoadString(data.c_str());
        return 1;
    }

    void BuildResult(val& rv, const Match &match)
    {
        // This vector will hold the points of recognized image
        std::vector<cv::Point2f> rectpoints;
        rectpoints.resize(4);
        match.myInput.myRect.points(&rectpoints[0]);
        
        rv.set("name", val(match.myDatabaseCard->myCardName));
        rv.set("id", val(match.myDatabaseCard->myCardId));
        rv.set("score", val((int)match.myScore[0]));
        rv.set("set", val(match.myDatabaseCard->mySetCode));
        rv.set("url", val(match.myDatabaseCard->myImgCoreUrl));
        
        // The rectangle in the image where the card was found
        rv.set("px0", val((int)rectpoints[0].x));
        rv.set("px1", val((int)rectpoints[1].x));
        rv.set("px2", val((int)rectpoints[2].x));
        rv.set("px3", val((int)rectpoints[3].x));
        rv.set("py0", val((int)rectpoints[0].y));
        rv.set("py1", val((int)rectpoints[1].y));
        rv.set("py2", val((int)rectpoints[2].y));
        rv.set("py3", val((int)rectpoints[3].y));
        rv.set("pointx", val((int)query.myPoint.x));
        rv.set("pointy", val((int)query.myPoint.y));
    }
    val AddScreen(const int &addr, int length, const size_t width, const size_t height)
    {
        uint8_t *data = reinterpret_cast<uint8_t *>(addr);
        Result r;
        int before = TimeNow();
        const bool success = query.AddScreenAndPrint(data, length, width, height, r);
        int after = TimeNow();
       // This is the JS object value returned to JS
        val rv(val::object());
        rv.set("success", val(success));
        rv.set("isautomatch", val(true));
        rv.set("time", val(after-before));
        if (success)
        {
            BuildResult(rv, r.myMatch.myList[0]);
        }
        return rv;
    }

    val FindCardInImage(const int &addr, int length, const size_t width, const size_t height)
    {
        uint8_t *data = reinterpret_cast<uint8_t *>(addr);
        Result r;
        int before = TimeNow();
        const bool success = query.FindCardInRoiAndPrint(data, length, width, height, r);
        int after = TimeNow();
        // This is the JS object value returned to JS
        val rv(val::object());
        rv.set("success", val(success));
        rv.set("isautomatch", val(false));
        rv.set("time", val(after-before));
         if (success)
        {
            BuildResult(rv, r.myMatch.myList[0]);
        }
        return rv;
    }
};

EMSCRIPTEN_BINDINGS(my_module)
{
    class_<CardSpotter>("CardSpotter")
        .constructor()
        .function("LoadDatabase", &CardSpotter::LoadDatabase)
        .function("SetSetting", &CardSpotter::SetSetting)
        .function("AddScreen", &CardSpotter::AddScreen, allow_raw_pointers())
        .function("FindCardInImage", &CardSpotter::FindCardInImage, allow_raw_pointers());
}
