#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <regex>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>

namespace fs = std::filesystem;

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

int main(int argc,char* argv[])
{
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << "  <path-to-SUSANNE/fc2>\n";
        return 1;
    }

    fs::path dir = argv[1];
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
                        std::vector<std::pair<long,char>> tri = {
                            {par.subjPos,'S'},{par.verbPos,'V'},{par.objPos,'O'}};
                        std::sort(tri.begin(),tri.end(),
                                  [](auto&a,auto&b){return a.first<b.first;});
                        std::string pat; for (auto&p:tri) pat.push_back(p.second);
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
                    std::vector<std::pair<long,char>> tri = {
                        {c.subjPos,'S'},{c.verbPos,'V'},{c.objPos,'O'}};
                    std::sort(tri.begin(),tri.end(),
                              [](auto&a,auto&b){return a.first<b.first;});
                    std::string pat; for (auto&p:tri) pat.push_back(p.second);
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
                            std::vector<std::pair<long,char>> tri = {
                                {fin.subjPos,'S'},{fin.verbPos,'V'},{fin.objPos,'O'}};
                            std::sort(tri.begin(),tri.end(),
                                      [](auto&a,auto&b){return a.first<b.first;});
                            std::string pat; for (auto&p:tri) pat.push_back(p.second);
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

    std::cout << "\n===== Word‑order distribution =====\n";
    if (grandTotal==0) { std::cout << "No complete S–V–O triples were found.\n"; return 0; }

    std::cout << std::fixed << std::setprecision(2);
    for (auto& kv : tally) {
        double pct = 100.0 * kv.second / grandTotal;
        std::cout << kv.first << '\t' << kv.second << '\t' << pct << "%\n";
    }
    return 0;
}
