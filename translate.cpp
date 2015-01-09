#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>
#include <vector>
#include <unordered_set>
#include <utility>

#include <fastcgi2/component.h>
#include <fastcgi2/component_factory.h>
#include <fastcgi2/handler.h>
#include <fastcgi2/request.h>

#include <mongo/client/dbclient.h>
#include <mongo/bson/bson.h>


using std::cout;
using std::cerr;
using std::endl;
using std::string;


namespace {

// Work with text below.


bool checkCommand(std::istream& uri, const string& value) {
    string token;
    std::getline(uri, token, '/'); // First string is empty.
    std::getline(uri, token, '/');
    return token == value;
}

std::vector<std::pair<string, string>> parseWordsPairsText(const string& text) {
    std::vector<std::pair<string, string>> result;
    std::stringstream textStr(text);
    string wordsPair;
    while (std::getline(textStr, wordsPair, '\n')) {
        if (wordsPair.empty()) {
            continue;
        }
        std::pair<string, string> p;
        {
            std::stringstream wordsPairStr(wordsPair);
            std::getline(wordsPairStr, p.first, ',');
            std::getline(wordsPairStr, p.second, ',');
        }
        result.push_back(p);
    }
    return result;
}

string buildDictName(const string& langFrom, const string& langTo) {
    return langFrom + "###" + langTo;
}

template <typename T>
void iterableToStream(const T& objects, std::ostream& out) {
    for (const auto& obj : objects) {
        out << obj << "\n";
    }
}


// Work with mongo below.

bool dbUpdate(const string& tableName, mongo::DBClientConnection& conn,
              const mongo::BSONObj& findCriteria, const mongo::BSONObj& newObj)
{
    try {
        conn.update(tableName, findCriteria, newObj, true);
    } catch (const mongo::DBException &e) {
        cerr << "update FAILED\t" << conn.getLastError() << std::endl;
        return false;
    }
    return true;
}

mongo::BSONObj dbFindOne(const string& tableName, mongo::DBClientConnection& conn,
                         const mongo::BSONObj& findCriteria, mongo::BSONObj* returnOnly = nullptr)
{
    mongo::BSONObj retObj;
    try {
        retObj = conn.findOne(tableName, findCriteria, returnOnly);
    } catch (const mongo::DBException &e) {
        cerr << "find FAILED\t" << conn.getLastError() << std::endl;
    }
    return retObj;
}

std::auto_ptr<mongo::DBClientCursor> dbFind(const string& tableName, mongo::DBClientConnection& conn,
                                            const mongo::BSONObj& findCriteria, mongo::BSONObj* returnOnly = nullptr)
{
    std::auto_ptr<mongo::DBClientCursor> cursor;
    try {
        cursor = conn.query(tableName, findCriteria, 0, 0, returnOnly);
    } catch (const mongo::DBException &e) {
        cerr << "find FAILED\t" << conn.getLastError() << std::endl;
    }
    return cursor;
}

} // anonymous namespace


class Translate
    : virtual public fastcgi::Component
    , virtual public fastcgi::Handler
{
public:
    Translate(fastcgi::ComponentContext* ctx)
        : fastcgi::Component(ctx)
    {
    }

    virtual void onLoad() override {
        mongo::client::initialize();
        try {
            DbConn.connect("localhost");
        } catch (const mongo::DBException &e) {
            cerr << "dbconnection FAILED(" << e.what() << ")" << endl;
        }
    }

    virtual void onUnload() override {
    }

    virtual void handleRequest(fastcgi::Request* req, fastcgi::HandlerContext* /* ctx */) override {
        std::stringstream resp;

        const auto METHOD = req->getRequestMethod();
        std::istringstream uri(req->getURI());

        if (METHOD == "GET") {
            if (!checkCommand(uri, "translate")) {
                req->sendError(400); // Bad request.
                return;
            }

            string langFrom, word, langTo;
            std::getline(uri, langFrom, '/');
            std::getline(uri, word, '/');
            std::getline(uri, langTo, '/');

            const int errCode = process(word, langFrom, langTo, resp);
            if (errCode != 0) {
                req->sendError(errCode);
                return;
            }
        } else if (METHOD == "POST") {
            if (!checkCommand(uri, "update")) {
                req->sendError(400); // Bad request.
                return;
            }
            string langFrom, langTo, wordsPairsText;
            std::getline(uri, langFrom, '/');
            std::getline(uri, langTo, '/');
            req->requestBody().toString(wordsPairsText);

            wordsPairsText = "a,a\nb,b\nc,c\nd,d\n\n\ne,e";
            langFrom = "enL";
            langTo = "enG";


            if (updateDict(langFrom, langTo, wordsPairsText)) {
                resp << "Dict was inserted/updated successfully.";
            } else {
                req->sendError(500); // Internal server error.
                return;
            }
        } else {
            req->sendError(405); // Method not allowed.
            return;
        }

        std::stringbuf buf(resp.str());
        req->write(&buf);
    }

private:
    int process(const string& word, const string& langFrom, const string& langTo, std::stringstream& out) {
        if (word.empty() && langFrom.empty() && langTo.empty()) {
            iterableToStream(getLangsFrom(), out);
            return 0;
        }
        if (!langTo.empty()) {
            if (langFrom.empty() || word.empty()) {
                return 400; // Bad request.
            }
            out << getTranslation(word, langFrom, langTo) << "\n";
            return 0;
        }
        if (!word.empty()) {
            if (langFrom.empty()) {
                return 400; // Bad request.
            }
            iterableToStream(getLangsTo(langFrom, word), out);
            return 0;
        }
        if (!langFrom.empty()) {
            iterableToStream(getWordsToTranslate(langFrom), out);
            return 0;
        } else {
            return 400; // Bad request.
        }
        return 0;
    }

    std::unordered_set<string> getLangsFrom() {
        mongo::BSONObj findCriteria = mongo::BSONObjBuilder()
            .append(LANG_FROM_KEY, BSON("$exists" << true))
            .obj();
        mongo::BSONObj returnOnly = mongo::BSONObjBuilder()
            .append(LANG_FROM_KEY, "")
            .obj();

        auto cursor = dbFind(TABLE_NAME, DbConn, findCriteria, &returnOnly);

        std::unordered_set<string> res;
        if (cursor.get() != nullptr) {
            while (cursor->more()) {
                const auto lang = cursor->next()[LANG_FROM_KEY].valuestrsafe();
                res.insert(lang);
            }
        }
        return res;
    }

    std::unordered_set<string> getLangsTo(const string& langFrom, const string& word) {
        mongo::BSONObj findCriteria = mongo::BSONObjBuilder()
            .append(LANG_FROM_KEY, langFrom)
            .append(word, BSON("$exists" << true))
            .obj();
        mongo::BSONObj returnOnly = mongo::BSONObjBuilder()
            .append(LANG_TO_KEY, "")
            .obj();

        auto cursor = dbFind(TABLE_NAME, DbConn, findCriteria, &returnOnly);

        std::unordered_set<string> res;
        if (cursor.get() != nullptr) {
            while (cursor->more()) {
                const auto lang = cursor->next()[LANG_TO_KEY].valuestrsafe();
                res.insert(lang);
            }
        }
        return res;
    }

    string getTranslation(const string& word, const string& langFrom, const string& langTo) {
        const auto dictName = buildDictName(langFrom, langTo);
        mongo::BSONObj findCriteria = mongo::BSONObjBuilder()
            .append(NAME_KEY, dictName)
            .obj();
        mongo::BSONObj returnOnly = mongo::BSONObjBuilder()
            .append(word, "")
            .obj();

        const mongo::BSONObj obj = dbFindOne(TABLE_NAME, DbConn, findCriteria, &returnOnly);
        return obj[word].valuestrsafe();
    }

    std::unordered_set<string> getWordsToTranslate(const string& langFrom) {
        mongo::BSONObj findCriteria = mongo::BSONObjBuilder()
            .append(LANG_FROM_KEY, langFrom)
            .obj();

        auto cursor = dbFind(TABLE_NAME, DbConn, findCriteria);

        std::unordered_set<string> res;
        if (cursor.get() != nullptr) {
            while (cursor->more()) {
                std::set<string> names;
                cursor->next().getFieldNames(names);
                for (const auto& name : names) {
                    if (KEYWORDS.find(name) == KEYWORDS.end()) {
                        res.insert(name);
                    }
                }
            }
        }
        return res;
    }

    bool updateDict(const string& langFrom, const string& langTo, const string& wordsPairsText) {
        const auto dictName = buildDictName(langFrom, langTo);
        const auto wordsPairs = parseWordsPairsText(wordsPairsText);

        mongo::BSONObj findCriteria = mongo::BSONObjBuilder()
            .append(NAME_KEY, dictName)
            .obj();

        mongo::BSONObj newObj;
        {
            mongo::BSONObjBuilder b;
            for (const auto& p : wordsPairs) {
                b.append(p.first, p.second);
            }
            newObj = b.append(NAME_KEY, dictName)
                .append(LANG_TO_KEY, langTo)
                .append(LANG_FROM_KEY, langFrom)
                .obj();
        }

        return dbUpdate(TABLE_NAME, DbConn, findCriteria, newObj);
    }

private:
    const string NAME_KEY = "__NAME__";
    const string LANG_TO_KEY = "__LANG_TO__";
    const string LANG_FROM_KEY = "__LANG_FROM__";
    const string MONGO_ID_KEY = "_id";
    const std::unordered_set<string> KEYWORDS = {NAME_KEY, LANG_TO_KEY, LANG_FROM_KEY, MONGO_ID_KEY};
    const string TABLE_NAME = "test.dicts";

public:
    mongo::DBClientConnection DbConn;
};

FCGIDAEMON_REGISTER_FACTORIES_BEGIN()
FCGIDAEMON_ADD_DEFAULT_FACTORY("translateFactory", Translate)
FCGIDAEMON_REGISTER_FACTORIES_END()