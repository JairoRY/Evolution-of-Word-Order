#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <regex> 
#include <map>

namespace fs = std::filesystem;

struct TokenData {
    std::string id;
    std::string posTag;
    std::string wordForm;
    std::string lemma;
    std::string syntacticAnnotation;
    std::string roleHint;            // "S", "V", "O", or empty/other based on heuristic
};

TokenData parseTokenLine(const std::string& line) {
    TokenData token;
    std::stringstream ss(line);
    std::string temp; 

  
    ss >> token.id >> temp >> token.posTag >> token.wordForm >> token.lemma;

    size_t annotationStart = line.find('[');
    if (annotationStart != std::string::npos) {
        token.syntacticAnnotation = line.substr(annotationStart);
    } else {
        token.syntacticAnnotation = ""; // No annotation found
    }
    return token;
}

/
std::string inferRoleHint(const TokenData& token) {
    // Look for explicit 'S' or 'O' in the bracketed annotation
    if (token.syntacticAnnotation.find("[S") != std::string::npos) {
        return "S";
    }
    if (token.syntacticAnnotation.find("[O") != std::string::npos) {
        return "O";
    }

    if (token.posTag.rfind("VV", 0) == 0 ||
        token.posTag.rfind("VB", 0) == 0 || 
        token.posTag == "MD")               
    {
        return "V";
    }

    // Default: if no clear S, O, or V role hint found
    return ""; // Or "X" for unknown/other
}


// --- 4. Main analysis function for each file ---
void analyzeFile(const fs::path& filePath, std::map<std::string, int>& counts) {
    std::ifstream file(filePath);
    std::string line;
    std::vector<TokenData> currentSentenceTokens; // Store parsed tokens for the current sentence

    while (std::getline(file, line)) {
        // Skip empty lines or lines that are not part of token sequence
        if (line.empty() || line.find("--- PAGE") != std::string::npos ||
            line.find("ANNUAL REVIEWS") != std::string::npos || // Skip document metadata lines if needed
            line.find("Keywords") != std::string::npos ||
            line.find("Abstract") != std::string::npos) {
            continue;
        }

        // Check for sentence start/end markers
        if (line.find("<s>") != std::string::npos) {
            currentSentenceTokens.clear(); // Clear tokens for the new sentence
            continue;
        }
        if (line.find("</s>") != std::string::npos) {
            // --- Process the completed sentence for patterns ---
            // Extract the sequence of role hints for this sentence
            std::vector<std::string> rolesInSentence;
            for (const auto& token : currentSentenceTokens) {
                if (!token.roleHint.empty()) { // Only consider tokens with an S, V, or O hint
                    rolesInSentence.push_back(token.roleHint);
                }
            }

            // Look for SVO, SOV, VSO trigrams in the sequence of role hints
            for (size_t i = 0; i + 2 < rolesInSentence.size(); ++i) {
                std::string trigram = rolesInSentence[i] + rolesInSentence[i+1] + rolesInSentence[i+2];
                if (trigram == "SOV") {
                    counts["SOV"]++;
                } else if (trigram == "SVO") {
                    counts["SVO"]++;
                } else if (trigram == "VSO") {
                    counts["VSO"]++;
                }
            }

            currentSentenceTokens.clear(); // Clear for the next sentence
            continue;
        }

        // If it's a token line, parse it
        TokenData parsedToken = parseTokenLine(line);
        parsedToken.roleHint = inferRoleHint(parsedToken); // Infer the role hint
        currentSentenceTokens.push_back(parsedToken);
    }
}

// --- 5. Main function to iterate through files and print results ---
int main() {
    std::string directoryPath = "./LUCYrf/CorrRF";
    std::map<std::string, int> patternCounts = { {"SOV", 0}, {"SVO", 0}, {"VSO", 0} };

    // Check if directory exists
    if (!fs::exists(directoryPath) || !fs::is_directory(directoryPath)) {
        std::cerr << "Error: Directory '" << directoryPath << "' does not exist or is not a directory.\n";
        return 1; // Indicate error
    }
    
    for (const auto& entry : fs::directory_iterator(directoryPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".vrt") { // Ensure it's a .vrt file
            std::cout << "Analyzing file: " << entry.path().filename() << "\n";
            analyzeFile(entry.path(), patternCounts);
        }
    }

    std::cout << "\n--- Pattern Counts ---\n";
    std::cout << "SOV: " << patternCounts["SOV"] << "\n";
    std::cout << "SVO: " << patternCounts["SVO"] << "\n";
    std::cout << "VSO: " << patternCounts["VSO"] << "\n";

    return 0;
}
