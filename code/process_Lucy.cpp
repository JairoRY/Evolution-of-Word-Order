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
    bool counted  = false;
    long fallbackObj = -1; // ★ fallback from :s in Po phrase
};

constexpr bool DEBUG = true;

const std::unordered_set<std::string> CLAUSE_TAGS = {
    "O","Oh","Ot","Q","I","Iq","Iu",
    "S","Ss","Fa","Fn","Fr","Ff","Fc",
    "Tg","Tn","Ti","Tf","Tb","Tq",
    "W","A","Z","L",
    "Po" // ★ include Po to track prepositional phrases
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
        if (!in) {
            std::cout << "Cannot open " << ent.path() << '\n';
            continue;
        }

        if (DEBUG) std::cout << "\n>>> Processing file: " << ent.path() << '\n';

        std::vector<Clause> stack;
        std::vector<std::string> poStack; // ★ track active Po blocks
        long tokIdx = 0;

        std::string line;
        while (std::getline(in,line))
        {
            if (line.empty()) { ++tokIdx; continue; }

            std::istringstream iss(line);
            std::vector<std::string> col;
            std::string word;
            while (iss >> word) col.push_back(std::move(word));
            if (col.size() < 6) { ++tokIdx; continue; }

            const std::string& pos        = col[2];
            const std::string& formtags   = col[5];

            // ------------------- opening brackets -------------------
            for (auto it = std::sregex_iterator(formtags.begin(),formtags.end(),OPEN_RE);
                 it != std::sregex_iterator(); ++it)
            {
                std::string fullTag = (*it)[1].str();
                size_t colon = fullTag.find(':');
                std::string baseTag = (colon == std::string::npos) ? fullTag
                                                                   : fullTag.substr(0, colon);

                if (!CLAUSE_TAGS.count(baseTag)) continue;

                if (baseTag == "Po") poStack.push_back("Po"); // ★ push Po marker

                if (!stack.empty() && colon != std::string::npos) {
                    Clause& parent = stack.back();
                    char role = fullTag[colon + 1];
                    if (role == 's' && parent.subjPos == -1) parent.subjPos = tokIdx;
                    if (role == 'o' && parent.objPos  == -1) parent.objPos  = tokIdx;

                    if (DEBUG && (role == 's' || role == 'o'))
                        std::cout << std::string(stack.size(), ' ')
                                  << "subclause-as-" << (role == 's' ? "subject" : "object")
                                  << " @tok " << tokIdx << '\n';

                    if (!parent.counted && parent.subjPos != -1 &&
                        parent.verbPos != -1 && parent.objPos != -1)
                    {
                        std::vector<std::pair<long,char>> tri = {
                            {parent.subjPos,'S'},{parent.verbPos,'V'},{parent.objPos,'O'}};
                        std::sort(tri.begin(),tri.end());
                        std::string pat; for (auto& p : tri) pat.push_back(p.second);
                        ++tally[pat]; ++grandTotal;
                        parent.counted = true;
                        if (DEBUG)
                            std::cout << std::string(stack.size(), ' ')
                                      << "order → " << pat << '\n';
                    }
                }

                stack.push_back({baseTag});
                if (DEBUG) std::cout << std::string(stack.size(), ' ')
                                     << "[OPEN] " << baseTag << "  @tok " << tokIdx << '\n';
            }

            // ------------------- token classification -------------------
            if (!stack.empty()) {
                Clause& c = stack.back();

                if (c.verbPos == -1 && !pos.empty() && (pos[0] == 'V' || pos[0] == 'v')) {
                    c.verbPos = tokIdx;
                    if (DEBUG) std::cout << std::string(stack.size(), ' ')
                                         << "verb   @tok " << tokIdx << '\n';
                }

                for (auto m = std::sregex_iterator(formtags.begin(),formtags.end(),ROLE_RE);
                     m != std::sregex_iterator(); ++m)
                {
                    std::string tagName = (*m)[1].str();
                    char role = (*m)[2].str()[0];

                    if (CLAUSE_TAGS.count(tagName)) continue;

                    if (role == 's') {
                        if (c.subjPos == -1) {
                            c.subjPos = tokIdx;
                            if (DEBUG) std::cout << std::string(stack.size(), ' ')
                                                 << "subject@tok " << tokIdx << '\n';
                        }

                        if (!poStack.empty() && c.objPos == -1 && c.fallbackObj == -1) {
                            c.fallbackObj = tokIdx;
                            if (DEBUG)
                                std::cout << std::string(stack.size(), ' ')
                                          << "inferred object (from :s in Po) @tok " << tokIdx << '\n';
                        }
                    }

                    if (role == 'o' && c.objPos == -1) {
                        c.objPos = tokIdx;
                        if (DEBUG) std::cout << std::string(stack.size(), ' ')
                                             << "object @tok " << tokIdx << '\n';
                    }
                }

                // ★ Use fallback if still missing :o
                if (c.objPos == -1 && c.fallbackObj != -1) {
                    c.objPos = c.fallbackObj;
                    if (DEBUG) std::cout << std::string(stack.size(), ' ')
                                         << "fallback object → @tok " << c.objPos << '\n';
                }

                if (!c.counted && c.subjPos != -1 && c.verbPos != -1 && c.objPos != -1) {
                    std::vector<std::pair<long,char>> tri = {
                        {c.subjPos,'S'},{c.verbPos,'V'},{c.objPos,'O'}};
                    std::sort(tri.begin(),tri.end());
                    std::string pat; for (auto& p : tri) pat.push_back(p.second);
                    ++tally[pat]; ++grandTotal;
                    c.counted = true;
                    if (DEBUG) std::cout << std::string(stack.size(), ' ')
                                         << "order → " << pat << '\n';
                }
            }

            // ------------------- deal with closing brackets -------------------
            for (auto it = std::sregex_iterator(formtags.begin(),formtags.end(),CLOSE_RE);
                 it != std::sregex_iterator(); ++it)
            {
                std::string close = (*it)[1].str();
                if (!CLAUSE_TAGS.count(close)) continue;

                if (close == "Po" && !poStack.empty()) poStack.pop_back(); // ★ pop Po

                while (!stack.empty()) {
                    Clause done = stack.back(); stack.pop_back();

                    if (done.tag == close) {
                        if (!done.counted &&
                            done.subjPos != -1 && done.verbPos != -1 && done.objPos != -1)
                        {
                            std::vector<std::pair<long,char>> tri = {
                                {done.subjPos,'S'},{done.verbPos,'V'},{done.objPos,'O'}};
                            std::sort(tri.begin(),tri.end());
                            std::string pat; for (auto& p : tri) pat.push_back(p.second);
                            ++tally[pat]; ++grandTotal;
                            if (DEBUG) std::cout << std::string(stack.size() + 2, ' ')
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
    if (grandTotal == 0) {
        std::cout << "No complete S–V–O triples were found.\n";
        return 0;
    }

    std::cout << std::fixed << std::setprecision(2);
    for (auto& kv : tally) {
        double pct = 100.0 * kv.second / grandTotal;
        std::cout << kv.first << '\t' << kv.second << '\t' << pct << "%\n";
    }

    return 0;
}
