#include <cpprest/http_client.h>
#include <cpprest/json.h>

using namespace web;
using namespace web::http;
using namespace web::http::client;

#include <iostream>
using namespace std;

void display_json(
    json::value const& jvalue,
    utility::string_t const& prefix)
{
    wcout << prefix << jvalue.serialize() << endl;
}

pplx::task<http_response> make_task_request(
    http_client& client,
    method mtd,
    json::value const& jvalue)
{
    return (mtd == methods::GET || mtd == methods::HEAD) ?
        client.request(mtd, L"/restdemo") :
        client.request(mtd, L"/restdemo", jvalue);
}

void make_request(
    http_client& client,
    method mtd,
    json::value const& jvalue)
{
    make_task_request(client, mtd, jvalue)
        .then([](http_response response)
            {
                if (response.status_code() == status_codes::OK)
                {
                    return response.extract_json();
                }
    return pplx::task_from_result(json::value());
            })
        .then([](pplx::task<json::value> previousTask)
            {
                try
            {
                display_json(previousTask.get(), L"R: ");
            }
            catch (http_exception const& e)
            {
                wcout << e.what() << endl;
            }
            })
                .wait();
}

int post()
{
    http_client client(U("http://localhost"));

    auto putvalue = json::value::object();
    putvalue[L"one"] = json::value::string(L"100");
    putvalue[L"two"] = json::value::string(L"200");

    wcout << L"\nPUT (add values)\n";
    display_json(putvalue, L"S: ");
    make_request(client, methods::PUT, putvalue);

    auto getvalue = json::value::array();
    getvalue[0] = json::value::string(L"one");
    getvalue[1] = json::value::string(L"two");
    getvalue[2] = json::value::string(L"three");

    wcout << L"\nPOST (get some values)\n";
    display_json(getvalue, L"S: ");
    make_request(client, methods::POST, getvalue);

    auto delvalue = json::value::array();
    delvalue[0] = json::value::string(L"one");

    wcout << L"\nDELETE (delete values)\n";
    display_json(delvalue, L"S: ");
    make_request(client, methods::DEL, delvalue);

    wcout << L"\nPOST (get some values)\n";
    display_json(getvalue, L"S: ");
    make_request(client, methods::POST, getvalue);

    auto nullvalue = json::value::null();

    wcout << L"\nGET (get all values)\n";
    display_json(nullvalue, L"S: ");
    make_request(client, methods::GET, nullvalue);

    return 0;
}