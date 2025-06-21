#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <regex>
#include <map>
#include <set>
#include <vector>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;
using namespace std;

// Trim whitespace
string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    return (start == string::npos) ? "" : s.substr(start, end - start + 1);
}

// Detect SVO order from SUSANNE-like parsed text
string detectOrderFromSUSANNE(const string& tree) {
    vector<string> lines;
    stringstream ss(tree);
    string line;
    while (getline(ss, line)) {
        lines.push_back(line);
    }

    vector<pair<char, size_t>> elements;

    // More forgiving regex for subject/object and verb tags
    regex subjectTag(R"(:[sS]\b)");
    regex objectTag(R"(:[oO]\b)");
    regex verbPOS(R"(\tV[BDZGNP][A-Za-z]*\t)");

    for (size_t i = 0; i < lines.size(); ++i) {
        const string& line = lines[i];

        // Split line into tab-separated fields
        vector<string> fields;
        stringstream lineStream(line);
        string token;
        while (getline(lineStream, token, '\t')) {
            fields.push_back(token);
        }

        if (fields.size() < 6) continue;

        string posTag = fields[3];     // POS tag field
        string parseField = fields[5]; // Parse field

        if (regex_search(parseField, subjectTag)) {
            elements.emplace_back('S', i);
        } else if (regex_search(parseField, objectTag)) {
            elements.emplace_back('O', i);
        } else if (regex_match("\t" + posTag + "\t", verbPOS)) {
            elements.emplace_back('V', i);
        }
    }

    // Sort elements by line position
    sort(elements.begin(), elements.end(), [](auto& a, auto& b) {
        return a.second < b.second;
    });

    // Construct order string without duplicates
    string order;
    set<char> seen;
    for (auto& [ch, _] : elements) {
        if (!seen.count(ch)) {
            order += ch;
            seen.insert(ch);
        }
    }

    return (order.size() == 3) ? order : "";
}

int main() {
    map<string, int> orderCounts;
    int total = 0;

    string dirPath = "pceec2-main/data/parsed/";
    for (const auto& entry : fs::directory_iterator(dirPath)) {
        if (!entry.is_regular_file()) continue;

        ifstream infile(entry.path());
        if (!infile) {
            cerr << "Failed to open file: " << entry.path() << endl;
            continue;
        }

        string line, buffer;
        while (getline(infile, line)) {
            buffer += line + "\n"; // preserve line breaks
            if (line.find("))") != string::npos) {  // crude sentence boundary
                string tree = trim(buffer);
                string order = detectOrderFromSUSANNE(tree);
                if (order.size() == 3) {
                    orderCounts[order]++;
                    total++;
                }
                buffer.clear();
            }
        }
    }

    // Print results
    cout << "Word Order Statistics:\n";
    vector<string> allOrders = {"SVO", "SOV", "VSO", "VOS", "OVS", "OSV"};
    for (const string& order : allOrders) {
        int count = orderCounts[order];
        double percent = (total > 0) ? 100.0 * count / total : 0.0;
        cout << order << ": " << count << " (" << percent << "%)\n";
    }

    return 0;
}
