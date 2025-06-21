/*
 * SUSANNE‑clause word‑order counter
 * ---------------------------------
 * Build:  g++ -std=c++17 -O2 susanne_wordorder.cpp -o susanne_wordorder
 * Usage:  ./susanne_wordorder /path/to/SUSANNE/fc2
 *
 * What it does
 * 1.  Reads every regular file under the directory you pass on the
 *     command line (recursively, so sub‑directories are fine).
 * 2.  For every token line it looks at
 *        – column  2 (POS / grammatical tag,   zero‑based index 1)
 *        – column  6 (constituent‑structure field, zero‑based index 5)
 * 3.  Column 6 is scanned for clause start tags like “[S”, “[Fa”, … and
 *     matching end tags “S]”, “Fa]”, … .  These open and close a stack
 *     of `Clause` objects.
 * 4.  While a clause is open the program records the first position of
 *        Subject  (tag containing “:s”)
 *        Verb     (tag whose first letter is ‘V’)
 *        Object   (tag containing “:o”)
 *     Position is simply the running token index in the file;
 *     comparing the three indices yields the word order.
 * 5.  When the closing tag of that clause arrives the S/V/O indices are
 *     sorted, turned into a string such as “SVO” or “VSO”, and a global
 *     counter is incremented.
 * 6.  After the last file has been processed the counters are turned
 *     into percentages and printed.
 *
 * Assumptions
 * • Column boundaries are whitespace‑separated.
 * • Column 2 really does contain the SUSANNE POS/grammatical tag
 *   (with :s and :o roles and verbs starting with ‘V’).
 * • Column 6 holds the bracketed form‑tags exactly as in the corpus
 *   documentation.
 * • Only clauses that contain at least one Subject, one Verb, *and*
 *   one Object are counted.
 * Adjust the column numbers or the role tests if your copy of the
 * corpus is laid out differently.
 */
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

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <path‑to‑SUSANNE/fc2>\n";
        return 1;
    }
    fs::path dir = argv[1];

    std::unordered_map<std::string, long> counts;   // SVO → n
    long tokenIndex = 0;                            // global running index

    std::regex startRe(R"(\[([A-Za-z]+))");         // “[S”, “[Fa” …
    std::regex endRe  (R"(([A-Za-z]+)\])");         // “S]”, “Fa]” …

    /* ------------------------------------------------------------------ */
    for (auto const& entry : fs::recursive_directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;

        std::ifstream in(entry.path());
        if (!in) {
            std::cerr << "Cannot open " << entry.path() << '\n';
            continue;
        }

        std::vector<Clause> stack;                  // open clauses
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;

            /* split the line into whitespace‑separated columns */
            std::istringstream iss(line);
            std::vector<std::string> cols;
            std::string cell;
            while (iss >> cell) cols.push_back(cell);
            if (cols.size() < 6) continue;          // malformed line

            const std::string& posTag   = cols[2];  // 2nd column
            const std::string& formTags = cols[5];  // 6th column

            /* ---- handle opening tags ------------------------------------------------ */
            for (std::sregex_iterator m(formTags.begin(), formTags.end(), startRe),
                                       me;
                 m != me; ++m)
            {
                Clause c;
                c.tag = (*m)[1].str();              // “S”, “Fa”, …
                stack.push_back(c);
            }

            /* ---- handle lexical roles while inside a clause ------------------------- */
            if (!stack.empty()) {
                Clause& c = stack.back();

                bool isVerb    = !posTag.empty() && posTag[0] == 'V';
                bool isSubject = posTag.find(":s") != std::string::npos;
                bool isObject  = posTag.find(":o") != std::string::npos;

                if (isSubject && c.subjectPos == -1) c.subjectPos = tokenIndex;
                if (isVerb    && c.verbPos    == -1) c.verbPos    = tokenIndex;
                if (isObject  && c.objectPos  == -1) c.objectPos  = tokenIndex;
            }

            /* ---- handle closing tags ------------------------------------------------ */
            for (std::sregex_iterator m(formTags.begin(), formTags.end(), endRe),
                                       me;
                 m != me; ++m)
            {
                std::string closing = (*m)[1].str();

                while (!stack.empty()) {
                    Clause finished = stack.back();
                    stack.pop_back();

                    if (finished.tag == closing) {       // correct closing tag
                        if (finished.subjectPos != -1 &&
                            finished.verbPos    != -1 &&
                            finished.objectPos  != -1)
                        {
                            /* build word‑order key */
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
                        }
                        break;      // clause closed
                    }
                }
            }

            ++tokenIndex;
        }
    }
    /* ------------------------------------------------------------------ */

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
