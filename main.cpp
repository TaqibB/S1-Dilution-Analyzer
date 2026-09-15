#include <iostream>
#include <string>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

size_t receiveData(void* contents, size_t size, size_t nmemb, void* userData) {

    std::string* response = static_cast<std::string*>(userData);

    size_t totalSize = size * nmemb;

    response->append(static_cast<char*>(contents), totalSize);

    return totalSize;
}

int main() {

    std::string response;

    CURL* curl = curl_easy_init();

    if (curl == nullptr) {
        std::cout << "Could not initialize curl." << std::endl;
        return 1;
    }


    curl_easy_setopt(curl, CURLOPT_URL,
        "https://data.sec.gov/submissions/CIK0001560293.json");

    curl_easy_setopt(curl, CURLOPT_USERAGENT,
        "Dilution Risk Tool youremail@email.com");

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receiveData);

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        std::cout << "Request failed: "
                  << curl_easy_strerror(result) << std::endl;
    }
    else {
    nlohmann::json data = nlohmann::json::parse(response);

    std::cout << "Company: " << data["name"] << std::endl;
    std::cout << "Ticker: " << data["tickers"][0] << std::endl;
    std::cout << "CIK: " << data["cik"] << std::endl;

    int newestS1 = -1;

    for (int i = 0; i < data["filings"]["recent"]["form"].size(); i++) {

        std::string form = data["filings"]["recent"]["form"][i];

        if (form == "S-1") {

        newestS1 = i;
        break;
        }
    }

    if (newestS1 != -1) {

        std::cout << "Most recent S-1 found!" << std::endl;

        std::cout << "Date: "
                << data["filings"]["recent"]["filingDate"][newestS1]
                << std::endl;

        std::cout << "Document: "
                << data["filings"]["recent"]["primaryDocument"][newestS1]
                << std::endl;

        std::cout << "Accession: "
                << data["filings"]["recent"]["accessionNumber"][newestS1]
                << std::endl;
    }
    else {

        std::cout << "No S-1 filings found." << std::endl;
    }
    }

        curl_easy_cleanup(curl);

        return 0;
}