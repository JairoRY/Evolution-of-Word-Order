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
#include <cassert> 

namespace fs = std::filesystem;
using namespace std;

static string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end   = s.find_last_not_of(" \t\n\r");
    return (start == string::npos) ? "" : s.substr(start, end - start + 1);
}

static string detectOrder(const string& tree) {
    vector<pair<char, size_t>> elements;

    regex subj(R"(\(NP-SBJ\b)");                 
    regex verb(R"(\((VB[DPZGN]?|BE[DPN]|BAG|HV[PDZ]?)\b)"); 
    regex obj (R"(\(NP-OB1\b|\(NP-OBJ\b)");    
    regex prd (R"(\((NP|ADJP|PP)-PRD\b)");   
    
    for (sregex_iterator it(tree.begin(),tree.end(),subj), end; it!=end; ++it)
        elements.emplace_back('S', it->position());
    for (sregex_iterator it(tree.begin(),tree.end(),verb), end; it!=end; ++it)
        elements.emplace_back('V', it->position());
    for (sregex_iterator it(tree.begin(),tree.end(),obj ), end; it!=end; ++it)
        elements.emplace_back('O', it->position());
    for (sregex_iterator it(tree.begin(),tree.end(),prd ), end; it!=end; ++it){
        elements.emplace_back('O', it->position());
        cout << "Matched PRD at: " << it->position() << '\n';  
    }

    if (none_of(elements.begin(),elements.end(),[](auto&p){return p.first=='O';})) {
        regex np(R"(\(NP\b)");
        vector<size_t> npPos;
        for (sregex_iterator it(tree.begin(),tree.end(),np), end; it!=end; ++it)
            npPos.push_back(it->position());
        if (npPos.size() >= 2)
            elements.emplace_back('O', npPos[1]);
    }

    sort(elements.begin(), elements.end(), [](auto&a,auto&b){return a.second<b.second;});

    string order; set<char> seen;
    for (auto& [ch,pos] : elements) if (!seen.count(ch)) { order+=ch; seen.insert(ch);}    
    return (order.size()==3) ? order : "";
}


static void run_unit_tests()
{
    assert(trim("  foo\n") == "foo");
    assert(trim("\tbar  ") == "bar");

    const string S  = "(NP-SBJ X)";         
    const string Vb = "(VB eats)";          
    const string O  = "(NP-OB1 Y)";          

    assert(detectOrder(S + Vb + O) == "SVO");            
    assert(detectOrder(S + O  + Vb) == "SOV");            
    assert(detectOrder(Vb+ S  + O ) == "VSO");            
    assert(detectOrder(Vb+ O  + S ) == "VOS");           

    assert(detectOrder(O + Vb + S ) == "OVS");            
    assert(detectOrder(O + S  + Vb) == "OSV");            

    string copular = "(NP-SBJ It) (BEZ is) (ADJP-PRD nice)";
    assert(detectOrder(copular) == "SVO");

    string fallback = "(NP-SBJ John) (VB loves) (NP Mary)"; 
    assert(detectOrder(fallback) == "SVO");

}

int main()
{
#ifndef SKIP_TESTS
    run_unit_tests();
#endif

    map<string,int> orderCounts; int total = 0;
    const string dirPath = "pceec2-main/data/parsed/";

    for (const auto& entry : fs::directory_iterator(dirPath)) {
        if (!entry.is_regular_file()) continue;
        ifstream infile(entry.path());
        if (!infile) { cerr << "Failed to open " << entry.path() << '\n'; continue; }

        string line, buffer;
        while (getline(infile,line)) {
            buffer += line + ' ';
            if (line.find("))") != string::npos) {
                string tree = trim(buffer);
                string order = detectOrder(tree);
                if (!order.empty()) { ++orderCounts[order]; ++total; }
                buffer.clear();
            }
        }
    }

    cout << "\nWord Order Statistics:\n";
    vector<string> allOrders = {"SVO","SOV","VSO","VOS","OVS","OSV"};
    ofstream csv("word_order_stats_PCEEC.csv"); csv << "Order,Count,Percent\n";

    for (const string& ord : allOrders) {
        int count = orderCounts[ord];
        double pct = (total ? 100.0*count/total : 0.0);
        cout << ord << ": " << count << " (" << fixed << setprecision(2) << pct << "% )\n";
        csv << ord << ',' << count << ',' << fixed << setprecision(2) << pct << '\n';
    }
    csv.close();
    return 0;
}
