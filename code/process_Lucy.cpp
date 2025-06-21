#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <regex>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <cassert>  

namespace fs = std::filesystem;


static std::string order_pattern(long subj, long verb, long obj)
{
    std::vector<std::pair<long,char>> tri = {{subj,'S'},{verb,'V'},{obj,'O'}};
    std::sort(tri.begin(),tri.end(),[](auto&a,auto&b){return a.first<b.first;});
    std::string pat; for (auto&p:tri) pat.push_back(p.second);
    return pat;
}


static void run_unit_tests()
{
    struct ClauseTest { long subjPos=-1, verbPos=-1, objPos=-1; bool counted=false; } ct;
    assert(ct.subjPos == -1 && ct.verbPos == -1 && ct.objPos == -1 && !ct.counted);


    assert(order_pattern(0,1,2) == "SVO"); 
    assert(order_pattern(0,2,1) == "SOV");
    assert(order_pattern(1,0,2) == "VSO");
    assert(order_pattern(2,1,1) == "VOS");
    assert(order_pattern(2,1,0) == "OVS");
    assert(order_pattern(1,2,0) == "OSV");

    const std::regex OPEN_RE  (R"(\[([A-Za-z]+(?::[a-z])?))");
    const std::regex CLOSE_RE (R"(([A-Za-z]+)\])");
    const std::regex ROLE_RE  (R"(\[([A-Za-z]+):([so]))");

    std::string open  = "[S:s]";
    std::string close = "S]";
    std::string role  = "[IP:o";

    auto itOpen = std::sregex_iterator(open.begin(),  open.end(),  OPEN_RE );
    auto itClose= std::sregex_iterator(close.begin(), close.end(), CLOSE_RE);
    auto itRole = std::sregex_iterator(role.begin(),  role.end(),  ROLE_RE );

    assert(itOpen  != std::sregex_iterator() && (*itOpen)[1].str()  == "S:s");
    assert(itClose != std::sregex_iterator() && (*itClose)[1].str() == "S"  );
    assert(itRole  != std::sregex_iterator() && (*itRole)[1].str()  == "IP" && (*itRole)[2].str()=="o");


    assert(order_pattern(3,7,5) == "SOV");

}

struct Clause {
    std::string tag;           
    long subjPos  = -1;      
    long verbPos  = -1;        
    long objPos   = -1;       
    bool  counted = false;     
};

constexpr bool DEBUG = false;

const std::unordered_set<std::string> CLAUSE_TAGS = {
    "O","Oh","Ot","Q","I","Iq","Iu",
    "S","Ss","Fa","Fn","Fr","Ff","Fc",
    "Tg","Tn","Ti","Tf","Tb","Tq",
    "W","A","Z","L"
};

const std::regex OPEN_RE (R"(\[([A-Za-z]+(?::[a-z])?))");
const std::regex CLOSE_RE(R"(([A-Za-z]+)\])");
const std::regex ROLE_RE (R"(\[([A-Za-z]+):([so]))");

int main()
{
#ifndef SKIP_TESTS
    run_unit_tests();
#endif

    fs::path dir = "SUSANNE/fc2";
    std::unordered_map<std::string,long> tally;
    long grandTotal = 0;

    for (auto const& ent : fs::recursive_directory_iterator(dir))
    {
        if (!ent.is_regular_file()) continue;
        std::ifstream in(ent.path());
        if (!in) { std::cout << "Cannot open " << ent.path() << '\n'; continue; }

        if (DEBUG) std::cout << "\n>>> Processing file: " << ent.path() << '\n';

        std::vector<Clause> stack;
        long tokIdx = 0;

        std::string line;
        while (std::getline(in,line))
        {
            if (line.empty()) { ++tokIdx; continue; }

            std::istringstream iss(line);
            std::vector<std::string> col;
            for (std::string w; iss >> w; ) col.push_back(std::move(w));
            if (col.size() < 6) { ++tokIdx; continue; }

            const std::string& pos      = col[2];   
            const std::string& formtags = col[5];  

            for (auto it = std::sregex_iterator(formtags.begin(),formtags.end(),OPEN_RE);
                 it != std::sregex_iterator(); ++it)
            {
                std::string fullTag = (*it)[1].str();
                size_t colon = fullTag.find(':');
                std::string baseTag = (colon==std::string::npos) ? fullTag
                                                                : fullTag.substr(0,colon);

                if (!CLAUSE_TAGS.count(baseTag)) continue;

                if (!stack.empty() && colon!=std::string::npos) {
                    Clause& par = stack.back();
                    char role = fullTag[colon+1];
                    if (role=='s' && par.subjPos==-1) par.subjPos = tokIdx;
                    if (role=='o' && par.objPos ==-1) par.objPos  = tokIdx;

                    if (DEBUG && (role=='s'||role=='o'))
                        std::cout << std::string(stack.size(),' ')
                                  << "subclause-as-" << (role=='s'?"subject":"object")
                                  << " @tok " << tokIdx << '\n';

                    if (!par.counted && par.subjPos!=-1 && par.verbPos!=-1 && par.objPos!=-1) {
                        std::string pat = order_pattern(par.subjPos, par.verbPos, par.objPos);
                        ++tally[pat]; ++grandTotal;
                        par.counted = true;                          
                        if (DEBUG)
                            std::cout << std::string(stack.size(),' ')
                                      << "order → " << pat << '\n';
                    }
                }
                stack.push_back({baseTag});
                if (DEBUG) std::cout << std::string(stack.size(),' ')
                                     << "[OPEN] " << baseTag << "  @tok " << tokIdx << '\n';
            }

            if (!stack.empty()) {
                Clause& c = stack.back();

                if (c.verbPos==-1 && !pos.empty() && (pos[0]=='V'||pos[0]=='v'))
                    c.verbPos = tokIdx;

                for (auto m = std::sregex_iterator(formtags.begin(),formtags.end(),ROLE_RE);
                     m != std::sregex_iterator(); ++m)
                {
                    std::string tagName = (*m)[1].str();
                    char role          = (*m)[2].str()[0];

                    if (CLAUSE_TAGS.count(tagName)) continue;  

                    if (role=='s' && c.subjPos==-1) c.subjPos = tokIdx;
                    if (role=='o' && c.objPos ==-1) c.objPos  = tokIdx;
                }

                if (!c.counted && c.subjPos!=-1 && c.verbPos!=-1 && c.objPos!=-1) {
                    std::string pat = order_pattern(c.subjPos, c.verbPos, c.objPos);
                    ++tally[pat]; ++grandTotal;
                    c.counted = true;                              
                    if (DEBUG) std::cout << std::string(stack.size(),' ')
                                         << "order → " << pat << '\n';
                }
            }

            for (auto it = std::sregex_iterator(formtags.begin(),formtags.end(),CLOSE_RE);
                 it != std::sregex_iterator(); ++it)
            {
                std::string close = (*it)[1].str();
                if (!CLAUSE_TAGS.count(close)) continue;

                while (!stack.empty()) {
                    Clause fin = stack.back(); stack.pop_back();
                    if (fin.tag == close) {
                        if (!fin.counted &&
                            fin.subjPos!=-1 && fin.verbPos!=-1 && fin.objPos!=-1)
                        {
                            std::string pat = order_pattern(fin.subjPos, fin.verbPos, fin.objPos);
                            ++tally[pat]; ++grandTotal;
                            if (DEBUG) std::cout << std::string(stack.size()+2,' ')
                                                 << "order → " << pat << '\n';
                        }
                        break;
                    }
                }
            }
            ++tokIdx;
        }
    }

    std::vector<std::string> orders = {"SVO","SOV","VSO","VOS","OVS","OSV"};

    std::cout << "\nWord Order Statistics:\n";
    std::ofstream csv("word_order_stats_Susanne.csv");          
    csv << "Order,Count,Percent\n";                     

    for (const std::string& pat : orders) {
        long count = tally[pat];
        double pct = (grandTotal > 0) ? 100.0 * count / grandTotal : 0.0;
        std::cout << pat << ": " << count << " (" << std::fixed
                  << std::setprecision(2) << pct << "% )\n";

        csv << pat << ',' << count << ','                 
            << std::fixed << std::setprecision(2)         
            << pct << '\n';                               
    }
    csv.close();                                        
    
    return 0;
}
