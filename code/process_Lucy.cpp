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
// ... [unchanged includes and setup above] ...

struct Clause {
    std::string tag;
    long subjPos  = -1;
    long verbPos  = -1;
    long objPos   = -1;
    bool counted  = false;

    long fallbackObj = -1;   // ★ used to store potential object from PP
};

const std::unordered_set<std::string> CLAUSE_TAGS = {
    "O","Oh","Ot","Q","I","Iq","Iu",
    "S","Ss","Fa","Fn","Fr","Ff","Fc",
    "Tg","Tn","Ti","Tf","Tb","Tq",
    "W","A","Z","L", "Po"  // ★ add Po to catch opening prepositional phrases
};

const std::regex OPEN_RE (R"(\[([A-Za-z]+(?::[a-z])?))");
const std::regex CLOSE_RE(R"(([A-Za-z]+)\])");
const std::regex ROLE_RE (R"(\[([A-Za-z]+):([so]))");

std::vector<std::string> poStack;  // ★ stack to track Po nesting

// ... inside your token-processing loop ...

/* ------------------- deal with opening brackets ------------------- */
for (auto it = std::sregex_iterator(formtags.begin(),formtags.end(),OPEN_RE);
     it != std::sregex_iterator(); ++it)
{
    std::string fullTag = (*it)[1].str();
    size_t colon = fullTag.find(':');
    std::string baseTag = (colon==std::string::npos) ? fullTag
                                                     : fullTag.substr(0,colon);

    if (!CLAUSE_TAGS.count(baseTag)) continue;

    // NEW: if we open a Po prepositional phrase, push to poStack
    if (baseTag == "Po") poStack.push_back("Po"); // ★

    if (!stack.empty() && colon!=std::string::npos) {
        Clause& parent = stack.back();
        char role = fullTag[colon+1];
        if (role == 's' && parent.subjPos == -1) parent.subjPos = tokIdx;
        if (role == 'o' && parent.objPos  == -1) parent.objPos  = tokIdx;

        if (DEBUG && (role == 's' || role == 'o'))
            std::cout << std::string(stack.size(),' ')
                      << "subclause-as-" << (role=='s'?"subject":"object")
                      << " @tok " << tokIdx << '\n';

        if (!parent.counted && parent.subjPos!=-1 && parent.verbPos!=-1 && parent.objPos!=-1) {
            std::vector<std::pair<long,char>> tri = {
                {parent.subjPos,'S'},{parent.verbPos,'V'},{parent.objPos,'O'}};
            std::sort(tri.begin(),tri.end(),
                      [](auto&a,auto&b){return a.first<b.first;});
            std::string pat; for (auto&p:tri) pat.push_back(p.second);
            ++tally[pat]; ++grandTotal;
            parent.counted = true;
            if (DEBUG)
                std::cout << std::string(stack.size(),' ')
                          << "order → " << pat << '\n';
        }
    }
    stack.push_back({baseTag});
    if (DEBUG) std::cout << std::string(stack.size(),' ')
                         << "[OPEN] " << baseTag << "  @tok " << tokIdx << '\n';
}

/* ------------------------ token classification -------------------- */
if (!stack.empty()) {
    Clause& c = stack.back();

    if (c.verbPos==-1 && !pos.empty() && (pos[0]=='V'||pos[0]=='v')) c.verbPos = tokIdx;

    for (auto m = std::sregex_iterator(formtags.begin(),formtags.end(),ROLE_RE);
         m != std::sregex_iterator(); ++m)
    {
        std::string tagName = (*m)[1].str();
        char role           = (*m)[2].str()[0];

        if (CLAUSE_TAGS.count(tagName)) continue;

        if (role == 's') {
            if (c.subjPos == -1) c.subjPos = tokIdx;

            // ★ NEW: if we're inside a [Po ...], remember this as potential object
            if (!poStack.empty() && c.objPos == -1 && c.fallbackObj == -1) {
                c.fallbackObj = tokIdx;
                if (DEBUG)
                    std::cout << std::string(stack.size(),' ')
                              << "inferred object (from :s in Po) @tok " << tokIdx << '\n';
            }
        }

        if (role == 'o' && c.objPos == -1) {
            c.objPos = tokIdx;
        }
    }

    // ★ Assign fallback object if still no :o but we have a good candidate
    if (!c.counted && c.objPos == -1 && c.fallbackObj != -1) {
        c.objPos = c.fallbackObj;
        if (DEBUG)
            std::cout << std::string(stack.size(),' ')
                      << "fallback object → @tok " << c.objPos << '\n';
    }

    // ★ Tally immediately if ready
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

/* ------------------- deal with closing brackets ------------------- */
for (auto it = std::sregex_iterator(formtags.begin(),formtags.end(),CLOSE_RE);
     it != std::sregex_iterator(); ++it)
{
    std::string close = (*it)[1].str();
    if (!CLAUSE_TAGS.count(close)) continue;

    if (close == "Po" && !poStack.empty()) poStack.pop_back(); // ★

    while (!stack.empty()) {
        Clause done = stack.back(); stack.pop_back();

        if (done.tag == close) {
            if (!done.counted &&
                done.subjPos!=-1 && done.verbPos!=-1 && done.objPos!=-1)
            {
                std::vector<std::pair<long,char>> tri = {
                    {done.subjPos,'S'},{done.verbPos,'V'},{done.objPos,'O'}};
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
