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

// Extract word order by finding S, V, O and sorting by position
string detectOrder(const string& tree) {
    vector<pair<char, size_t>> elements;

    // Match subject
    regex subj(R"(\(NP-SBJ\b)");
    for (sregex_iterator it(tree.begin(), tree.end(), subj), end; it != end; ++it)
        elements.emplace_back('S', it->position());

    // Match main verbs: VB*, BE (for copular), HV* (have forms)
    regex verb(R"(\((VB[DPZGN]?|BE[DPN]|BAG|HV[PDZ]?)\b)");
    for (sregex_iterator it(tree.begin(), tree.end(), verb), end; it != end; ++it)
        elements.emplace_back('V', it->position());

    // Match direct object (NP-OB1 or NP-OBJ)
    regex obj(R"(\(NP-OB1\b|\(NP-OBJ\b)");
    for (sregex_iterator it(tree.begin(), tree.end(), obj), end; it != end; ++it)
        elements.emplace_back('O', it->position());

    // Match predicative complements (as possible O in copular clauses)
    regex prd(R"(\((NP|ADJP|PP)-PRD\b)");
    for (sregex_iterator it(tree.begin(), tree.end(), prd), end; it != end; ++it)
        elements.emplace_back('O', it->position());

    // If no explicit object or predicative complement found,
    // fallback: use second NP as object
    if (count_if(elements.begin(), elements.end(), [](auto& p){ return p.first == 'O'; }) == 0) {
        regex np(R"(\(NP\b)");
        vector<size_t> np_positions;
        for (sregex_iterator it(tree.begin(), tree.end(), np), end; it != end; ++it)
            np_positions.push_back(it->position());
        if (np_positions.size() >= 2)
            elements.emplace_back('O', np_positions[1]);  // second NP = object
    }

    // Sort by position in the string
    sort(elements.begin(), elements.end(), [](auto& a, auto& b) {
        return a.second < b.second;
    });

    // Build word order string
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
            buffer += line + " ";
            if (line.find("))") != string::npos) {
                string tree = trim(buffer);
                string order = detectOrder(tree);
                if (order.size() == 3) {
                    orderCounts[order]++;
                    total++;
                }
                buffer.clear();
            }
        }
    }

   

    cout << "\nWord Order Statistics:\n";
    vector<string> allOrders = {"SVO", "SOV", "VSO", "VOS", "OVS", "OSV"};

    ofstream csv("word_order_stats_PCEEC.csv"); 
    csv << "Order,Count,Percent\n";        

    for (const string& order : allOrders) {
        int count = orderCounts[order];
        double percent = (total > 0) ? 100.0 * count / total : 0.0;

        cout << order << ": " << count << " (" << fixed << setprecision(2)
             << percent << "%)\n";

        csv << order << "," << count << "," << fixed << setprecision(2)
            << percent << "\n"; 
    }

    csv.close(); 

    return 0;
} 
