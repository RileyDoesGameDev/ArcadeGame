#pragma once
#include <string>
#include <vector>
#include <algorithm>

// ---------------------------------------------------------------------------
// LeaderboardEntry
// ---------------------------------------------------------------------------
struct LeaderboardEntry {
    std::string name;
    int         score = 0;

    // Sort descending by score
    bool operator<(const LeaderboardEntry& o) const { return score > o.score; }
};

// ---------------------------------------------------------------------------
// Leaderboard
//   Stores the top-N scores and persists them to a simple CSV file.
//   File format (one entry per line):  NAME,SCORE
// ---------------------------------------------------------------------------
class Leaderboard {
public:
    static constexpr int MAX_ENTRIES = 10;

    explicit Leaderboard(const std::string& filepath = "scores.csv");

    // Load scores from disk (call once at startup)
    bool load();

    // Persist scores to disk
    bool save() const;

    // Add a new entry; automatically trims to MAX_ENTRIES and sorts
    void addEntry(const std::string& name, int score);

    // Returns whether this score qualifies for the board
    bool isHighScore(int score) const;

    // Read-only access to the sorted list
    const std::vector<LeaderboardEntry>& entries() const { return m_entries; }

private:
    std::string                   m_filepath;
    std::vector<LeaderboardEntry> m_entries;
};
