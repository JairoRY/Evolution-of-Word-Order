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

/* ------------------------------------------------------------------ */
/*                           data structures                          */
/* ------------------------------------------------------------------ */
struct Clause {
    std::string tag;           // S, Fa, O ...
    long subjPos  = -1;        // first subject token inside clause
    long verbPos  = -1;        // first verb
    long objPos   = -1;        // first object
};

constexpr bool DEBUG = true;     // toggle all diagnostic output

/* ------------------------------------------------------------------ */
/*                    clause tags we really care about                */
/* ------------------------------------------------------------------ */
const std::unordered_set<std::string> CLAUSE_TAGS = {
    /* root‑rank */
    "O","Oh","Ot","Q","I","Iq","Iu",
    /* finite / non‑finite clauses */
    "S","Ss","Fa","Fn","Fr","Ff","Fc",
    "Tg","Tn","Ti","Tf","Tb","Tq",
    "W","A","Z","L"
};

/* ------------------------------------------------------------------ */
int main(int argc,char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << "  <path-to-SUSANNE/fc2>\n";
        return 1;
    }

    fs::path dir = argv[1];
    std::unordered_map<std::string,long> tally;   // e.g. “SVO” → 57
    long grandTotal = 0;

    /* regex to pull [TAG  ...  TAG] substrings out of col‑6 */
    const std::regex  OPEN_RE(R"(\[([A-Za-z]+))");
    const std::regex CLOSE_RE(R"(([A-Za-z]+)\])");

    for (auto const& ent : fs::recursive_directory_iterator(dir))
    {
        if (!ent.is_regular_file()) continue;
        std::ifstream in(ent.path());
        if (!in) {
            std::cerr << "Cannot open " << ent.path() << '\n';
            continue;
        }

        if (DEBUG) std::cerr << "\n>>> Processing file: " << ent.path() << '\n';

        std::vector<Clause> stack;
        long tokIdx = 0;                       // token counter *per file*

        std::string line;
        while (std::getline(in,line))
        {
            if (line.empty()) { ++tokIdx; continue; }

            /* split on whitespace – SUSANNE’s six columns are tab/space‑separated */
            std::istringstream iss(line);
            std::vector<std::string> col;
            std::string word;
            while (iss >> word) col.push_back(word);
            if (col.size() < 6) { ++tokIdx; continue; }

            const std::string& pos        = col[1];   // column 2  (POS / sub‑pos)
            const std::string& formtags   = col[5];   // column 6  (brackets & roles)

            /* ------------------- deal with opening brackets ------------------- */
            for (auto it = std::sregex_iterator(formtags.begin(),formtags.end(),OPEN_RE);
                 it != std::sregex_iterator(); ++it)
            {
                const std::string tag = (*it)[1].str();
                if (!CLAUSE_TAGS.count(tag)) continue;          // ignore non‑clause labels

                stack.push_back({tag});
                if (DEBUG) std::cerr << std::string(stack.size(),' ')
                                     << "[OPEN] " << tag << "  @tok " << tokIdx << '\n';
            }

            /* ------------------------ token classification -------------------- */
            if (!stack.empty()) {
                Clause& c = stack.back();

                /* 1. verb?  – very simple rule: POS starts with 'V' */
                if (c.verbPos == -1 && !pos.empty() && (pos[0] == 'V' || pos[0]=='v')) {
                    c.verbPos = tokIdx;
                    if (DEBUG) std::cerr << std::string(stack.size(),' ')
                                         << "verb   @tok " << tokIdx << '\n';
                }
                /* 2. subject?  – look for “:s” in formtag field */
                if (c.subjPos == -1 && formtags.find(":s") != std::string::npos) {
                    c.subjPos = tokIdx;
                    if (DEBUG) std::cerr << std::string(stack.size(),' ')
                                         << "subject@tok " << tokIdx << '\n';
                }
                /* 3. object? */
                if (c.objPos  == -1 && formtags.find(":o") != std::string::npos) {
                    c.objPos = tokIdx;
                    if (DEBUG) std::cerr << std::string(stack.size(),' ')
                                         << "object @tok " << tokIdx << '\n';
                }
            }

            /* ------------------- deal with closing brackets ------------------- */
            for (auto it = std::sregex_iterator(formtags.begin(),formtags.end(),CLOSE_RE);
                 it != std::sregex_iterator(); ++it)
            {
                const std::string closeTag = (*it)[1].str();
                if (!CLAUSE_TAGS.count(closeTag)) continue;

                /* pop stack until we find the matching opener */
                while (!stack.empty()) {
                    Clause done = stack.back();
                    stack.pop_back();

                    if (DEBUG) std::cerr << std::string(stack.size()+1,' ')
                                         << "[CLOSE] " << done.tag << " @tok " << tokIdx << '\n';

                    if (done.tag == closeTag) {
                        if (done.subjPos != -1 && done.verbPos != -1 && done.objPos != -1) {
                            /* produce ordering key (e.g. “SOV”) */
                            std::vector<std::pair<long,char>> trip = {
                                {done.subjPos,'S'}, {done.verbPos,'V'}, {done.objPos,'O'} };
                            std::sort(trip.begin(),trip.end(),
                                      [](auto& a,auto& b){ return a.first < b.first; });
                            std::string pattern;
                            for (auto& p : trip) pattern.push_back(p.second);

                            ++tally[pattern];
                            ++grandTotal;

                            if (DEBUG) std::cerr << std::string(stack.size()+2,' ')
                                                 << "order → " << pattern << '\n';
                        }
                        break;          // finished matching current CLOSE tag
                    }
                }
            }

            ++tokIdx;
        }
    }

    /* ------------------------------------------------------------------ */
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
