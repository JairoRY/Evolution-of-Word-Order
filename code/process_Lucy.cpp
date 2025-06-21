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
#include <algorithm> // For std::sort and std::find_if
#include <iomanip>   // For std::fixed and std::setprecision

namespace fs = std::filesystem;
using namespace std;

// Define clause-level tags more precisely based on the provided legend.
// These are the tags that indicate a complete clause structure, not just any phrase.
const string CLAUSE_TAG_PATTERN = "S|Fa|Fn|Fr|Ff|Fc|Tg|Tn|Ti|Tf|Tb|Tq|W|A|Z|L";
const regex clauseStartRegex(R"(\[(" + CLAUSE_TAG_PATTERN + R")\b)"); // e.g., [S, [Fa
const regex clauseEndRegex(R"(\b(" + CLAUSE_TAG_PATTERN + R")\])");   // e.g., S], Fa]

// Trims whitespace
string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    return (start == string::npos) ? "" : s.substr(start, end - start + 1);
}

// Detects order inside a clause
string detectOrderFromSUSANNE(const vector<string>& lines, const string& clauseType) {
    if (lines.empty()) return "";

    vector<pair<char, size_t>> elements;
    regex subjectTag(R"(:[s]\b)");
    regex objectTag(R"(:[o]\b)");
    // Added more comprehensive verb POS tags from SUSANNE documentation
    regex verbPOS(R"(^(Vb[dn]s?|Vd[ip]|Vn[pt]|Vp[in]|Vr[dp]|Vs[op]|Vt[dp]|Vz[fp]?|VH[DZS]?|VM[0do]?))");

    cout << "[DEBUG] Analyzing clause (type: " << clauseType << ") with " << lines.size() << " lines.\n";
    // For detailed debugging, uncomment the following block to print clause content
    /*
    cout << "--- Clause Content Start ---\n";
    for (const string& l : lines) {
        cout << l << "\n";
    }
    cout << "--- Clause Content End ---\n";
    */

    for (size_t i = 0; i < lines.size(); ++i) {
        const string& line = lines[i];

        vector<string> fields;
        stringstream ss(line);
        string token;
        while (getline(ss, token, '\t')) {
            fields.push_back(token);
        }
        if (fields.size() < 6) continue; // Malformed line or non-data line

        string posTag = fields[3];
        string parseField = fields[5];

        // We only care about the first S, V, O found within the current clause lines
        // Check if 'S' is already added to avoid duplicates if multiple subject tags on different lines
        if (regex_search(parseField, subjectTag) && find_if(elements.begin(), elements.end(), [](const auto& p){ return p.first == 'S'; }) == elements.end()) {
            elements.emplace_back('S', i);
            cout << "[DEBUG] Subject found on line " << i << " within clause (type: " << clauseType << ").\n";
        }
        // Check if 'O' is already added
        if (regex_search(parseField, objectTag) && find_if(elements.begin(), elements.end(), [](const auto& p){ return p.first == 'O'; }) == elements.end()) {
            elements.emplace_back('O', i);
            cout << "[DEBUG] Object found on line " << i << " within clause (type: " << clauseType << ").\n";
        }
        // Check if 'V' is already added
        if (regex_search(posTag, verbPOS) && find_if(elements.begin(), elements.end(), [](const auto& p){ return p.first == 'V'; }) == elements.end()) {
            elements.emplace_back('V', i);
            cout << "[DEBUG] Verb found on line " << i << " within clause (type: " << clauseType << ").\n";
        }
    }

    sort(elements.begin(), elements.end(), [](auto& a, auto& b) {
        return a.second < b.second;
    });

    string order;
    set<char> seen; // Use a set to ensure unique S, V, O in order
    for (auto& [ch, _] : elements) {
        if (!seen.count(ch)) {
            order += ch;
            seen.insert(ch);
        }
    }

    if (order.size() == 3) {
        cout << "[DEBUG] Detected clause order for " << clauseType << ": " << order << "\n";
    } else {
        cout << "[INFO] No complete SVO order (found " << order.length() << " elements: " << order << ") found for clause type: " << clauseType << ".\n";
    }
    return (order.size() == 3) ? order : "";
}

int main() {
    map<string, int> orderCounts;
    int total = 0;

    string dirPath = "SUSANNE/fc2"; // Ensure this path is correct and accessible
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        cerr << "Error: Directory not found or not accessible: " << dirPath << endl;
        return 1;
    }

    for (const auto& entry : fs::directory_iterator(dirPath)) {
        if (!entry.is_regular_file()) continue;

        cout << "\n[DEBUG] Reading file: " << entry.path() << "\n";
        ifstream infile(entry.path());
        if (!infile) {
            cerr << "Failed to open file: " << entry.path() << endl;
            continue;
        }

        string line;
        // Stack to store clause tag and the starting line index of its *content* in the global buffer
        stack<pair<string, size_t>> clauseStack;
        vector<string> globalLinesBuffer; // Buffer to hold all lines of the current file being processed

        while (getline(infile, line)) {
            globalLinesBuffer.push_back(line);
            size_t currentLineIndexInGlobalBuffer = globalLinesBuffer.size() - 1;

            vector<string> fields;
            stringstream ss(line);
            string token;
            while (getline(ss, token, '\t')) {
                fields.push_back(token);
            }
            string parseField = (fields.size() >= 6) ? fields[5] : "";

            // --- Process closing tags first to ensure inner clauses are processed before outer ones ---
            // Use sregex_iterator to find all matches for clauseEndRegex on the current line.
            // Collect them and reverse to process innermost closing tags first.
            vector<string> closingTagsOnLine;
            for (sregex_iterator it(parseField.begin(), parseField.end(), clauseEndRegex), end; it != end; ++it) {
                closingTagsOnLine.push_back((*it)[1].str());
            }
            reverse(closingTagsOnLine.begin(), closingTagsOnLine.end()); 

            for (const string& endTag : closingTagsOnLine) {
                if (!clauseStack.empty() && clauseStack.top().first == endTag) {
                    pair<string, size_t> completedClause = clauseStack.top();
                    clauseStack.pop();

                    cout << "[DEBUG] Clause END matched: " << completedClause.first << " at global line " << currentLineIndexInGlobalBuffer << ".\n";
                    
                    // Extract lines belonging to this completed clause.
                    // Slice from the global buffer using the stored start index and current line index.
                    vector<string> clauseLines;
                    for (size_t i = completedClause.second; i <= currentLineIndexInGlobalBuffer; ++i) {
                        clauseLines.push_back(globalLinesBuffer[i]);
                    }

                    if (!clauseLines.empty()) {
                        string order = detectOrderFromSUSANNE(clauseLines, completedClause.first);
                        if (!order.empty()) {
                            orderCounts[order]++;
                            total++;
                        }
                    }
                } else {
                    cout << "[WARNING] Mismatched or unexpected clause end tag: " << endTag << " at global line " << currentLineIndexInGlobalBuffer << ".\n";
                }
            }

            // --- Process opening tags ---
            // Use sregex_iterator to find all matches for clauseStartRegex on the current line.
            for (sregex_iterator it_start(parseField.begin(), parseField.end(), clauseStartRegex), end_start; it_start != end_start; ++it_start) {
                string startTag = (*it_start)[1].str();
                clauseStack.push({startTag, currentLineIndexInGlobalBuffer}); // Push tag and its starting line index
                cout << "[DEBUG] Clause START detected: [" << startTag << " in file " << entry.path().filename() << " at global line " << currentLineIndexInGlobalBuffer << "]\n";
            }
        } // End of while (getline(infile, line))

        // After reading all lines, process any remaining open clauses (should be none for well-formed files)
        while (!clauseStack.empty()) {
            pair<string, size_t> remainingClause = clauseStack.top();
            clauseStack.pop();
            cerr << "[WARNING] Unclosed clause: " << remainingClause.first << " started at global line " << remainingClause.second << " in file: " << entry.path().filename() << endl;
            // Process any unclosed clauses with lines from their start to the end of the file
            vector<string> clauseLines;
            for (size_t i = remainingClause.second; i < globalLinesBuffer.size(); ++i) {
                clauseLines.push_back(globalLinesBuffer[i]);
            }
            if (!clauseLines.empty()) {
                string order = detectOrderFromSUSANNE(clauseLines, remainingClause.first);
                if (!order.empty()) {
                    orderCounts[order]++;
                    total++;
                }
            }
        }
    } // End of for (const auto& entry : fs::directory_iterator(dirPath))

    // Print results
    cout << "\n=== Word Order Statistics (Clause Level) ===\n";
    vector<string> allOrders = {"SVO", "SOV", "VSO", "VOS", "OVS", "OSV"};
    for (const string& order : allOrders) {
        int count = orderCounts[order];
        double percent = (total > 0) ? 100.0 * count / total : 0.0;
        cout << order << ": " << count << " (" << fixed << setprecision(2) << percent << "%)\n";
    }

    return 0;
}
