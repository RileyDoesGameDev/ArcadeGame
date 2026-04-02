#include "Leaderboard.h"
#include <fstream>
#include <sstream>
#include <iostream>

Leaderboard::Leaderboard(const std::string& filepath)
    : m_filepath(filepath) {}

// ---------------------------------------------------------------------------
bool Leaderboard::load() {
    m_entries.clear();

    std::ifstream file(m_filepath);
    if (!file.is_open()) {
        // No file yet – that's fine on first run
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::istringstream ss(line);
        std::string name, scoreStr;

        if (std::getline(ss, name, ',') && std::getline(ss, scoreStr)) {
            try {
                LeaderboardEntry e;
                e.name  = name;
                e.score = std::stoi(scoreStr);
                m_entries.push_back(e);
            } catch (...) {
                std::cerr << "[Leaderboard] Skipping malformed line: " << line << '\n';
            }
        }
    }

    std::sort(m_entries.begin(), m_entries.end());
    if (m_entries.size() > MAX_ENTRIES)
        m_entries.resize(MAX_ENTRIES);

    return true;
}

// ---------------------------------------------------------------------------
bool Leaderboard::save() const {
    std::ofstream file(m_filepath);
    if (!file.is_open()) {
        std::cerr << "[Leaderboard] Could not write to " << m_filepath << '\n';
        return false;
    }

    for (const auto& e : m_entries) {
        file << e.name << ',' << e.score << '\n';
    }
    return true;
}

// ---------------------------------------------------------------------------
void Leaderboard::addEntry(const std::string& name, int score) {
    m_entries.push_back({ name, score });
    std::sort(m_entries.begin(), m_entries.end());
    if (static_cast<int>(m_entries.size()) > MAX_ENTRIES)
        m_entries.resize(MAX_ENTRIES);
    save();
}

// ---------------------------------------------------------------------------
bool Leaderboard::isHighScore(int score) const {
    if (static_cast<int>(m_entries.size()) < MAX_ENTRIES) return true;
    return score > m_entries.back().score;
}
