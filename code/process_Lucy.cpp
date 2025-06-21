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
#include <algorithm> // For std::sort, std::reverse, and std::find_if
#include <iomanip>   // For std::fixed and std::setprecision

namespace fs = std::filesystem;
using namespace std;

// Clause tags as defined in the user's problem description.
const string CLAUSE_TAG_PATTERN = "S|Fa|Fn|Fr|Ff|Fc|Tg|Tn|Ti|Tf|Tb|Tq|W|A|Z|L";

// Corrected Regex to find the start of a clause, e.g., [S or [Fa:
// This version puts the pattern inside the capturing group.
const regex clauseStartRegex(R"(\[(` + CLAUSE_TAG_PATTERN + R")(?![A-Za-z0-9_]))");

// Corrected Regex to find the end of a clause, e.g., S] or :Fa]
// This version avoids the unsupported lookbehind `(?<!)` and correctly captures the tag.
const regex clauseEndRegex(R"((?:[^A-Za-z0-9_]|^)(` + CLAUSE_TAG_PATTERN + R")\])");

/**
 * @brief Analyzes a set of lines belonging to a single clause to determine the order of Subject (S), Verb (V), and Object (O).
 *
 * @param lines The vector of strings, where each string is a full line from the input file within the clause.
 * @param clauseType The type of clause being analyzed (e.g., "S", "Fn").
 * @return A string representing the order (e.g., "SVO", "VSO") if all three elements are found, otherwise an empty string.
 */
string detectOrderFromSUSANNE(const vector<string>& lines, const string& clauseType) {
    if (lines.empty()) return "";

    vector<pair<char, size_t>> elements;
    // Regex to find subject markers like ':s' in the parse field.
    regex subjectTag(R"(:s\b)");
    // Regex to find object markers like ':o' in the parse field.
    regex objectTag(R"(:o\b)");
    // A comprehensive regex for various verb POS tags based on the SUSANNE corpus standard.
    regex verbPOS(R"(^(Vb[dn]s?|Vd[ip]|Vn[pt]|Vp[in]|Vr[dp]|Vs[op]|Vt[dp]|Vz[fp]?|VH[DZS]?|VM[0do]?))");

    // Iterate through each line of the clause to find S, V, and O tokens.
    for (size_t i = 0; i < lines.size(); ++i) {
        const string& line = lines[i];

        vector<string> fields;
        stringstream ss(line);
        string token;
        while (getline(ss, token, '\t')) {
            fields.push_back(token);
        }
        if (fields.size() < 6) continue; // Skip malformed or non-token lines.

        string posTag = fields[3];
        string parseField = fields[5];

        // Use find_if to add only the *first* occurrence of S, O, and V.
        if (regex_search(parseField, subjectTag) && find_if(elements.begin(), elements.end(), [](const auto& p){ return p.first == 'S'; }) == elements.end()) {
            elements.emplace_back('S', i);
        }
        if (regex_search(parseField, objectTag) && find_if(elements.begin(), elements.end(), [](const auto& p){ return p.first == 'O'; }) == elements.end()) {
            elements.emplace_back('O', i);
        }
        if (regex_search(posTag, verbPOS) && find_if(elements.begin(), elements.end(), [](const auto& p){ return p.first == 'V'; }) == elements.end()) {
            elements.emplace_back('V', i);
        }
    }

    // The order is determined by the line number, assuming one token per line.
    sort(elements.begin(), elements.end(), [](const auto& a, const auto& b) {
        return a.second < b.second;
    });

    string order;
    for (const auto& [ch, pos] : elements) {
        order += ch;
    }

    // Only return a valid order if exactly one S, one V, and one O were found.
    return (order.length() == 3) ? order : "";
}

int main() {
    map<string, int> orderCounts;
    int totalClauses = 0;

    // --- IMPORTANT: Update this path to your data directory ---
    string dirPath = "SUSANNE/fc2";
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        cerr << "Error: Directory not found or not accessible: " << dirPath << endl;
        return 1;
    }

    for (const auto& entry : fs::directory_iterator(dirPath)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".tgs") continue; // Example: process only .tgs files

        cout << "\n--- Processing file: " << entry.path().filename() << " ---\n";
        ifstream infile(entry.path());
        if (!infile) {
            cerr << "Failed to open file: " << entry.path() << endl;
            continue;
        }

        string line;
        stack<pair<string, size_t>> clauseStack;
        vector<string> fileLinesBuffer; // Store all lines from the current file

        // Read the entire file into a buffer first
        while (getline(infile, line)) {
            fileLinesBuffer.push_back(line);
        }
        infile.close();

        // Now process the buffered lines
        for (size_t lineIdx = 0; lineIdx < fileLinesBuffer.size(); ++lineIdx) {
            const string& currentLine = fileLinesBuffer[lineIdx];

            vector<string> fields;
            stringstream ss(currentLine);
            string token;
            while (getline(ss, token, '\t')) {
                fields.push_back(token);
            }
            string parseField = (fields.size() >= 6) ? fields[5] : "";
            if (parseField.empty()) continue;


            // 1. Process closing tags first to correctly handle nested clauses on the same line.
            vector<string> closingTagsOnLine;
            for (sregex_iterator it(parseField.begin(), parseField.end(), clauseEndRegex), end; it != end; ++it) {
                closingTagsOnLine.push_back((*it)[1].str());
            }
            // Reverse to handle inner closes before outer ones (e.g., in `...V]S]`)
            reverse(closingTagsOnLine.begin(), closingTagsOnLine.end());

            for (const string& endTag : closingTagsOnLine) {
                if (clauseStack.empty()) {
                    cerr << "[WARNING] Mismatched closing tag '" << endTag << "' with no open clause on stack. File: "
                         << entry.path().filename() << ", Line: " << lineIdx + 1 << endl;
                    continue;
                }

                pair<string, size_t> openClause = clauseStack.top();
                if (openClause.first == endTag) {
                    clauseStack.pop(); // Found a match, pop it.

                    // Extract clause lines from buffer
                    vector<string> clauseLines;
                    if (openClause.second <= lineIdx) {
                        for (size_t i = openClause.second; i <= lineIdx; ++i) {
                            clauseLines.push_back(fileLinesBuffer[i]);
                        }
                    }

                    if (!clauseLines.empty()) {
                        string order = detectOrderFromSUSANNE(clauseLines, openClause.first);
                        if (!order.empty()) {
                            orderCounts[order]++;
                            totalClauses++;
                        }
                    }
                } else {
                    // This indicates a malformed structure, e.g., [S [Fn ... S].
                    cerr << "[WARNING] Mismatched closing tag. Expected '" << openClause.first << "' but found '" << endTag
                         << "'. File: " << entry.path().filename() << ", Line: " << lineIdx + 1 << endl;
                }
            }

            // 2. Process opening tags
            vector<string> openingTagsOnLine;
            for (sregex_iterator it(parseField.begin(), parseField.end(), clauseStartRegex), end; it != end; ++it) {
                openingTagsOnLine.push_back((*it)[1].str());
            }

            for (const string& startTag : openingTagsOnLine) {
                clauseStack.push({startTag, lineIdx}); // Push tag and its starting line index
            }
        }

        // After reading all lines, report any clauses that were never closed.
        while (!clauseStack.empty()) {
            pair<string, size_t> unclosedClause = clauseStack.top();
            clauseStack.pop();
            cerr << "[WARNING] Unclosed clause '" << unclosedClause.first << "' started on line "
                 << unclosedClause.second + 1 << " in file: " << entry.path().filename() << endl;
        }
    }

    // --- Print Final Results ---
    cout << "\n\n======== FINAL WORD ORDER STATISTICS ========\n";
    vector<string> allOrders = {"SVO", "SOV", "VSO", "VOS", "OVS", "OSV"};
    cout << "Total clauses with complete SVO elements found: " << totalClauses << "\n\n";

    for (const string& order : allOrders) {
        int count = orderCounts.count(order) ? orderCounts[order] : 0;
        double percent = (totalClauses > 0) ? (100.0 * count / totalClauses) : 0.0;
        cout << order << ":\t" << count << "\t(" << fixed << setprecision(2) << percent << "%)\n";
    }

    return 0;
}
