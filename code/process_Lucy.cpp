#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <regex>
#include <map>
#include <set>
#include <vector>
#include <stack>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;
using namespace std;

// Clause tag regex
const regex clauseStart(R"(\[(S|Ss|Fa|Fn|Fr|Ff|Fc|Tg|Tn|Ti|Tf|Tb|Tq|W|A|Z|L))");
const regex clauseEnd(R"((S|Ss|Fa|Fn|Fr|Ff|Fc|Tg|Tn|Ti|Tf|Tb|Tq|W|A|Z|L)\])");

// Trims whitespace
string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    return (start == string::npos) ? "" : s.substr(start, end - start + 1);
}

// Detects order inside a clause
string detectOrderFromSUSANNE(const vector<string>& lines) {
    vector<pair<char, size_t>> elements;
    regex subjectTag(R"(:[s]\b)");
    regex objectTag(R"(:[o]\b)");
    regex verbPOS(R"(^VV[BDGNZ]?|^VM|^VH)");

    for (size_t i = 0; i < lines.size(); ++i) {
        const string& line = lines[i];

        // Tokenize by tab
        vector<string> fields;
        stringstream ss(line);
        string token;
        while (getline(ss, token, '\t')) {
            fields.push_back(token);
        }
        if (fields.size() < 6) continue;

        string posTag = fields[3];
        string parseField = fields[5];

        if (regex_search(parseField, subjectTag)) {
            elements.emplace_back('S', i);
            cout << "[DEBUG] Subject found on line " << i << ": " << line << "\n";
        } else if (regex_search(parseField, objectTag)) {
            elements.emplace_back('O', i);
            cout << "[DEBUG] Object found on line " << i << ": " << line << "\n";
        } else if (regex_search(posTag, verbPOS)) {
            elements.emplace_back('V', i);
            cout << "[DEBUG] Verb found on line " << i << ": " << line << "\n";
        }
    }

    sort(elements.begin(), elements.end(), [](auto& a, auto& b) {
        return a.second < b.second;
    });

    string order;
    set<char> seen;
    for (auto& [ch, _] : elements) {
        if (!seen.count(ch)) {
            order += ch;
            seen.insert(ch);
        }
    }

    if (order.size() == 3) {
        cout << "[DEBUG] Clause order: " << order << "\n";
    }
    return (order.size() == 3) ? order : "";
}

int main() {
    map<string, int> orderCounts;
    int total = 0;

    string dirPath = "SUSANNE/fc2";
    for (const auto& entry : fs::directory_iterator(dirPath)) {
        if (!entry.is_regular_file()) continue;

        cout << "\n[DEBUG] Reading file: " << entry.path() << "\n";
        ifstream infile(entry.path());
        if (!infile) {
            cerr << "Failed to open file: " << entry.path() << endl;
            continue;
        }

        string line;
        vector<string> buffer;
        stack<string> clauseStack;
        vector<string> currentClause;

        while (getline(infile, line)) {
            buffer.push_back(line);
            smatch match;

            // Push clause starts
            if (regex_search(line, match, clauseStart)) {
                clauseStack.push(match.str(1));
                currentClause.push_back(line);
            }
            // Within a clause
            else if (!clauseStack.empty()) {
                currentClause.push_back(line);
            }

            // Pop clause ends
            if (regex_search(line, match, clauseEnd)) {
                if (!clauseStack.empty()) {
                    string closed = match.str(1);
                    if (clauseStack.top() == closed) {
                        clauseStack.pop();
                    }
                }

                if (clauseStack.empty() && !currentClause.empty()) {
                    cout << "\n[DEBUG] Clause completed. Lines:\n";
                    for (const string& l : currentClause) cout << l << "\n";

                    string order = detectOrderFromSUSANNE(currentClause);
                    if (!order.empty()) {
                        orderCounts[order]++;
                        total++;
                    }
                    currentClause.clear();
                }
            }
        }
    }

    // Print results
    cout << "\n=== Word Order Statistics (Clause Level) ===\n";
    vector<string> allOrders = {"SVO", "SOV", "VSO", "VOS", "OVS", "OSV"};
    for (const string& order : allOrders) {
        int count = orderCounts[order];
        double percent = (total > 0) ? 100.0 * count / total : 0.0;
        cout << order << ": " << count << " (" << percent << "%)\n";
    }

    return 0;
}
