#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <regex>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace fs = std::filesystem;

struct Clause {
    std::string tag;           // S, Fa, …
    long subjectPos = -1;
    long verbPos    = -1;
    long objectPos  = -1;
};

constexpr bool DEBUG = true;

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <path‑to‑SUSANNE/fc2>\n";
        return 1;
    }
    fs::path dir = argv[1];

    std::unordered_map<std::string, long> counts;
    long tokenIndex = 0;

    std::regex startRe(R"(\[([A-Za-z]+))");
    std::regex endRe  (R"(([A-Za-z]+)\])");

    for (auto const& entry : fs::recursive_directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;

        std::ifstream in(entry.path());
        if (!in) {
            std::cerr << "Cannot open " << entry.path() << '\n';
            continue;
        }

        if (DEBUG) std::cerr << "\n>>> Processing file: " << entry.path() << "\n";

        std::vector<Clause> stack;
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;

            std::istringstream iss(line);
            std::vector<std::string> cols;
            std::string cell;
            while (iss >> cell) cols.push_back(cell);
            if (cols.size() < 6) continue;

            const std::string& posTag   = cols[2];
            const std::string& formTags = cols[5];

            for (std::sregex_iterator m(formTags.begin(), formTags.end(), startRe),
                                       me;
                 m != me; ++m)
            {
                Clause c;
                c.tag = (*m)[1].str();
                stack.push_back(c);
                if (DEBUG) {
                    std::cerr << std::string(stack.size(), ' ')
                              << "[OPEN] Clause [" << c.tag << "] at token #" << tokenIndex << '\n';
                }
            }

            if (!stack.empty()) {
                Clause& c = stack.back();

                bool isVerb    = !posTag.empty() && posTag[0] == 'V';
                bool isSubject = posTag.find(":s") != std::string::npos;
                bool isObject  = posTag.find(":o") != std::string::npos;

                if (isSubject && c.subjectPos == -1) {
                    c.subjectPos = tokenIndex;
                    if (DEBUG) std::cerr << std::string(stack.size(), ' ') << "Found Subject at " << tokenIndex << '\n';
                }
                if (isVerb && c.verbPos == -1) {
                    c.verbPos = tokenIndex;
                    if (DEBUG) std::cerr << std::string(stack.size(), ' ') << "Found Verb at " << tokenIndex << '\n';
                }
                if (isObject && c.objectPos == -1) {
                    c.objectPos = tokenIndex;
                    if (DEBUG) std::cerr << std::string(stack.size(), ' ') << "Found Object at " << tokenIndex << '\n';
                }
            }

            for (std::sregex_iterator m(formTags.begin(), formTags.end(), endRe),
                                       me;
                 m != me; ++m)
            {
                std::string closing = (*m)[1].str();

                while (!stack.empty()) {
                    Clause finished = stack.back();
                    stack.pop_back();

                    if (DEBUG) {
                        std::cerr << std::string(stack.size()+1, ' ')
                                  << "[CLOSE] Clause [" << finished.tag << "] at token #" << tokenIndex << '\n';
                    }

                    if (finished.tag == closing) {
                        if (finished.subjectPos != -1 &&
                            finished.verbPos    != -1 &&
                            finished.objectPos  != -1)
                        {
                            std::vector<std::pair<long,char>> triplet = {
                                {finished.subjectPos, 'S'},
                                {finished.verbPos,    'V'},
                                {finished.objectPos,  'O'}
                            };
                            std::sort(triplet.begin(), triplet.end(),
                                      [](auto& a, auto& b) {
                                          return a.first < b.first;
                                      });
                            std::string key;
                            for (auto& p : triplet) key.push_back(p.second);
                            ++counts[key];

                            if (DEBUG) {
                                std::cerr << std::string(stack.size()+2, ' ')
                                          << ">> Detected word order: " << key << '\n';
                            }
                        }
                        break;
                    }
                }
            }

            ++tokenIndex;
        }
    }

    long grandTotal = 0;
    for (auto& kv : counts) grandTotal += kv.second;

    std::cout << "\n===== Word‑order distribution in " << dir << " =====\n";
    if (grandTotal == 0) {
        std::cout << "No complete S–V–O clauses found.\n";
        return 0;
    }

    std::cout << std::fixed << std::setprecision(2);
    for (auto& kv : counts) {
        double pct = 100.0 * kv.second / grandTotal;
        std::cout << kv.first << '\t' << kv.second
                  << '\t' << pct << "%\n";
    }
    return 0;
}
