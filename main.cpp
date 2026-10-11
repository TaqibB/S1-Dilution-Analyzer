#include <iostream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <algorithm>

size_t receiveData(void* contents, size_t size, size_t nmemb, void* userData) {

    std::string* response = static_cast<std::string*>(userData);

    size_t totalSize = size * nmemb;

    response->append(static_cast<char*>(contents), totalSize);

    return totalSize;
}

int main() {

    std::string cik;

    std::cout << "Enter company CIK: ";
    std::cin >> cik;

    while (cik.length() < 10) {
        cik = "0" + cik;
    }

    std::string response;
    //Using Qwen 3.5
    std::string aiResponse;

    CURL* curl = curl_easy_init();

    if (curl == nullptr) {
        std::cout << "Could not initialize curl." << std::endl;
        return 1;
    }


    std::string secURL =
    "https://data.sec.gov/submissions/CIK" + cik + ".json";

curl_easy_setopt(curl, CURLOPT_URL, secURL.c_str());

    curl_easy_setopt(curl, CURLOPT_USERAGENT,
        "Dilution Risk Tool contact@example.com");

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

        std::cout << "Most recent S-1 found" << std::endl;

        std::cout << "Date: "
                << data["filings"]["recent"]["filingDate"][newestS1]
                << std::endl;

        std::cout << "Document: "
                << data["filings"]["recent"]["primaryDocument"][newestS1]
                << std::endl;

        std::cout << "Accession: "
                << data["filings"]["recent"]["accessionNumber"][newestS1]
                << std::endl;


        std::string accession = data["filings"]["recent"]["accessionNumber"][newestS1];

        std::string document = data["filings"]["recent"]["primaryDocument"][newestS1];

        std::string cik = data["cik"];

        std::string cleanAccession = "";

        for (int j = 0; j < accession.length(); j++) {

            if (accession[j] != '-') {
                cleanAccession += accession[j];
            }
        }

        std::string url = "https://www.sec.gov/Archives/edgar/data/"  + std::to_string(std::stoll(cik)) + "/" + cleanAccession + "/" + document;

        std::cout << "S-1 URL: " << url << std::endl;

        //Download S-1
        std::string filingResponse;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &filingResponse);

        CURLcode filingResult = curl_easy_perform(curl);

        if (filingResult != CURLE_OK) {

            std::cout << "Filing request failed: " << curl_easy_strerror(filingResult) << std::endl;
        }
        else {

                std::cout << "S-1 downloaded successfully!" << std::endl;

                std::cout << "Filing size: "
                << filingResponse.length()
                << " characters"
                << std::endl;

                // Different phrases companies may use for warrants
                std::vector<std::string> warrantTerms = {
                "outstanding warrants",
                "warrants outstanding",
                "warrants to purchase",
                "exercise of warrants",
                "shares underlying warrants"
                };

                std::vector<std::string> priceTerms = {
                "exercise price of",
                "exercise price equal to",
                "exercise price",
                "weighted average exercise price"
                };

                std::vector<size_t> warrantPositions;
                std::vector<size_t> pricePositions;

                // Search for warrant information
                for (int i = 0; i < warrantTerms.size(); i++) {
                    size_t position = filingResponse.find(warrantTerms[i]);

                    while (position != std::string::npos) {
                        warrantPositions.push_back(position);
                        position = filingResponse.find(warrantTerms[i], position + 1);
                    }
                }

                // Search for exercise prices
                for (int i = 0; i < priceTerms.size(); i++) {
                    size_t position = filingResponse.find(priceTerms[i]);

                    while (position != std::string::npos) {
                        pricePositions.push_back(position);
                        position = filingResponse.find(priceTerms[i], position + 1);
                    }
                }

                std::cout << "Warrant matches: " << warrantPositions.size() << std::endl;
                std::cout << "Exercise price matches: " << pricePositions.size() << std::endl;

                if (!warrantPositions.empty()) {

                    std::cout << "Warrant information found!" << std::endl;

                    //AI Request using qwen3.5
                    nlohmann::json aiRequest;

                    aiRequest["model"] = "qwen3.5:4b";

                    std::string priceText = "";
                    std::vector<std::pair<int, std::string>> rankedExcerpts;
                    //prompt
                    std::string warrantText = "";

                for (int i = 0; i < warrantPositions.size(); i++) {

                    size_t start = 0;

                    if (warrantPositions[i] > 300) {
                        start = warrantPositions[i] - 300;
                    }

                    std::string excerpt = filingResponse.substr(start, 600);

                    int score = 0;

                    if (excerpt.find("outstanding warrants") != std::string::npos) {
                        score += 3;
                    }

                    if (excerpt.find("weighted average exercise price") != std::string::npos) {
                        score += 3;
                    }

                    if (excerpt.find("shares of common stock issuable") != std::string::npos) {
                        score += 2;
                    }

                    if (excerpt.find("exercise price") != std::string::npos) {
                        score += 1;

                        if (excerpt.find("warrant") != std::string::npos ||
                            excerpt.find("Warrant") != std::string::npos) {
                            score += 3;
                        }
                    }

                    std::cout << "Excerpt " << i + 1
                        << " relevance score: " << score << std::endl;

                    rankedExcerpts.push_back({score, excerpt});
                }

                std::sort(rankedExcerpts.begin(), rankedExcerpts.end(),
                [](const auto& a, const auto& b) {
                 return a.first > b.first;
                    }
                );

                std::vector<std::string> selectedKeys;
                int selectedCount = 0;

                for (int i = 0; i < rankedExcerpts.size() && selectedCount < 3; i++) {

                    std::string excerpt = rankedExcerpts[i].second;

                    size_t keyStart = excerpt.find("outstanding warrants");

                    if (keyStart == std::string::npos) {
                        keyStart = excerpt.find("exercise price");
                    }

                    if (keyStart == std::string::npos) {
                        keyStart = 0;
                    }

                    std::string key = excerpt.substr(keyStart, 100);

                    bool duplicate = false;

                    for (const std::string& previousKey : selectedKeys) {
                        if (key == previousKey) {
                            duplicate = true;
                            break;
                        }
                    }

                    if (duplicate) {
                        std::cout << "Skipped duplicate excerpt\n";
                        continue;
                    }

                    selectedKeys.push_back(key);

                    warrantText += "\n--- Warrant Excerpt ---\n";
                    warrantText += excerpt;

                    selectedCount++;

                    std::cout << "Selected excerpt score: "
                            << rankedExcerpts[i].first << std::endl;
                }

                int priceCount = 0;

                for (int i = 0; i < pricePositions.size() && priceCount < 3; i++) {

                    size_t start = 0;

                    if (pricePositions[i] > 300) {
                        start = pricePositions[i] - 300;
                    }

                    std::string excerpt = filingResponse.substr(start, 600);

                    priceText += "\n--- Exercise Price Excerpt ---\n";
                    priceText += excerpt;

                    priceCount++;
                }

                std::cout << "\n===== EXERCISE PRICE TEXT =====\n";
                std::cout << priceText << std::endl;
                std::cout << "===== END EXERCISE PRICE TEXT =====\n";

                std::cout << "Selected exercise price excerpts: "
                    << priceCount << std::endl;

                std::cout << "\n===== TEXT SENT TO QWEN =====\n";
                std::cout << warrantText << std::endl;
                std::cout << "===== END OF TEXT =====\n";

                    std::string prompt =
                        "You are extracting warrant information from an SEC S-1 filing. "
                        "Prioritize statements that explicitly describe the total number "
                        "of shares issuable upon exercise of outstanding warrants. "
                        "If the filing provides a weighted-average exercise price "
                        "for those outstanding warrants, use that price. "
                        "Do not add together historical warrant issuances. "
                        "Do not include warrants proposed in the current offering. "
                        "Do not confuse stock options or RSUs with warrants. "
                        "Return only valid JSON with these fields: "
                        "\"active_warrants\", \"warrant_shares\", \"exercise_price\". "
                        "Use true for active_warrants only when outstanding warrants "
                        "are explicitly confirmed. "
                        "If the provided excerpts do not establish whether warrants "
                        "are outstanding, return null for active_warrants, not false. "
                        "Use false only if the filing explicitly confirms there are "
                        "no outstanding warrants. "
                        "Use null when the information cannot be determined. "
                        "Do not guess.\n\n"
                        "Exercise prices may differ between warrant types. "
                        "Only return a single exercise price if the evidence supports "
                        "using that price for the reported warrant shares. "
                        "Otherwise return null for exercise_price. "
                        "Outstanding warrant excerpts:\n"
                        + warrantText
                        + "\n\nExercise price excerpts:\n"
                        + priceText;

                     aiRequest["prompt"] = prompt;



                    aiRequest["stream"] = false;
                    aiRequest["think"] = false;

                    std::string requestBody = aiRequest.dump();

                    struct curl_slist* headers = nullptr;

                    headers = curl_slist_append(
                        headers,
                        "Content-Type: application/json"
                    );

                    curl_easy_setopt(curl, CURLOPT_URL,
                        "http://localhost:11434/api/generate");

                    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
                        requestBody.c_str());

                    curl_easy_setopt(curl, CURLOPT_WRITEDATA,
                        &aiResponse);

                    CURLcode aiResult = curl_easy_perform(curl);

                    if (aiResult != CURLE_OK) {

                        std::cout << "AI request failed: "
                                << curl_easy_strerror(aiResult)
                                << std::endl;
                    }
                    else {

                    nlohmann::json aiData =
                        nlohmann::json::parse(aiResponse);

                    std::string aiText = aiData["response"];

                    nlohmann::json warrantData =
                        nlohmann::json::parse(aiText);


                    std::cout << "Active Warrants: ";

                    if (warrantData["active_warrants"] == true) {
                        std::cout << "Yes" << std::endl;
                    }
                    else {
                        std::cout << "No" << std::endl;
                    }

                    std::cout << "Warrant Shares: "
                        << warrantData["warrant_shares"]
                        << std::endl;

                    if (warrantData["exercise_price"].is_null()) {
                        std::cout << "Exercise Price: Unknown" << std::endl;
                    } else {
                        std::cout << "Exercise Price: $" << warrantData["exercise_price"] << std::endl;
                    }

                    }

                    curl_slist_free_all(headers);

                        }
                else {

                    std::cout << "Outstanding warrant information not found." << std::endl;
                }
        }

    }
    else {

        std::cout << "No S-1 filings found." << std::endl;
    }

    }


        curl_easy_cleanup(curl);

        return 0;
}