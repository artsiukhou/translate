#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>

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
using std::stringstream;


namespace {

bool checkCommand(std::istream& uri, const string& value) {
    string token;
    std::getline(uri, token, '/'); // First string is empty.
    std::getline(uri, token, '/');
    return token == value;
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
            cerr << "connection FAILED" << endl;
        }
    }

    virtual void onUnload() override {
    }

    virtual void handleRequest(fastcgi::Request* req, fastcgi::HandlerContext* /* ctx */) override {
        stringstream resp;

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

            /// TODO work with DB.
        } else if (METHOD == "POST") {
            if (!checkCommand(uri, "update")) {
                req->sendError(400); // Bad request.
                return;
            }
            string langFrom, langTo, wordsPairsText;
            std::getline(uri, langFrom, '/');
            std::getline(uri, langTo, '/');
            req->requestBody().toString(wordsPairsText);

            /// TODO work with DB.
        } else {
            req->sendError(405); // Method not allowed.
            return;
        }

        req->setContentType("application/json");
        std::stringbuf buf(resp.str());
        req->write(&buf);
    }

public:
    mongo::DBClientConnection DbConn;
};

FCGIDAEMON_REGISTER_FACTORIES_BEGIN()
FCGIDAEMON_ADD_DEFAULT_FACTORY("translateFactory", Translate)
FCGIDAEMON_REGISTER_FACTORIES_END()