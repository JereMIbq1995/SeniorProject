#ifndef JVC_STATUS
#define JVC_STATUS

#include <iostream>
#include <string>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <vector>
#include <set>
#include <map>
// #include <windows.h>
#include "FileEntry.h"
#include "JvcDao.h"
using namespace std;

// This should be a member variable of the functionality class once completed
class JvcStatus
{

private:
    JvcDao objReader;

    void traverseAll(filesystem::path currentPath, vector<string> &newFiles, const set<string> &ignores)
    {

        filesystem::directory_iterator it(currentPath);

        for (const filesystem::directory_entry &dir_entry : it)
        {
            if (ignores.find(dir_entry.path().filename().u8string()) == ignores.end())
            {
                if (!dir_entry.is_directory())
                {
                    newFiles.push_back(dir_entry.path().u8string());
                }
                else
                {
                    traverseAll(dir_entry.path(), newFiles, ignores);
                }
            }
        }
    }

    void traverseTreeWithDir(filesystem::path currentPath, string treeName, vector<string> &newFiles, vector<string> &deletedFiles,
                             vector<string> &modifiedFiles, const set<string> &ignores)
    {
        // Get all the fileNames and foldernames mentioned in the current tree
        map<string, FileEntry> entries;
        objReader.getTreeEntries(entries, treeName);

        // TODO: Implement this...
        filesystem::directory_iterator it(currentPath);

        // for anything that exists in currentPath:
        for (const filesystem::directory_entry dir_entry : it)
        {
            // If the folder is not one of the ignored ones, explore it
            if (ignores.find(dir_entry.path().filename().u8string()) == ignores.end())
            {
                filesystem::path entry_path = dir_entry.path();
                // If it's not in the tree, add to NEW
                map<string, FileEntry>::iterator matchPos = entries.find(entry_path.filename().u8string());
                if (matchPos == entries.end())
                {
                    newFiles.push_back(entry_path.u8string());
                }
                // If it's also in the tree:
                else
                {
                    // If it's a folder, recursively call traverseTreeWithDir on the path and the treeName
                    if (dir_entry.is_directory())
                    {
                        traverseTreeWithDir(entry_path, (matchPos->second).codeName, newFiles, deletedFiles, modifiedFiles, ignores);
                    }
                    // If it's a file, compare content, if different, add to MODIFIED
                    else
                    {
                        if (objReader.diff(dir_entry.path().u8string(), matchPos->second.codeName))
                        {
                            modifiedFiles.push_back(entry_path.u8string());
                        }
                    }
                }
            }
        }

        // If something exists in the tree, but not in the currentPath, add to DELETED
        for (auto const &item : entries)
        {
            bool notFoundInCurrent = true;
            filesystem::directory_iterator tempDirIt(currentPath);
            for (const filesystem::directory_entry dir_entry : tempDirIt)
            {
                if (item.first == dir_entry.path().filename().u8string())
                {
                    notFoundInCurrent = false;
                    break;
                }
            }
            if (notFoundInCurrent)
            {
                string separator = "\\";
                if (currentPath.u8string() == ".\\")
                {
                    separator = "";
                }
                deletedFiles.push_back(currentPath.u8string() + separator + item.first);
            }
        }
    }

    void getStatus(filesystem::path currentPath, vector<string> &newFiles, vector<string> &deletedFiles,
                   vector<string> &modifiedFiles, const set<string> &ignores)
    {

        // For now there's only master. However, this should change
        // based on watch the current branch is.

        // If there is no commit tracker, that means all files are untracked
        if (!filesystem::exists("./.jvc/head/master"))
        {
            traverseAll(currentPath, newFiles, ignores);
        }

        // If the commit tracker for the current branch exists
        else
        {
            // TODO: traverse and compare file to its blob...
            string mostRecentVersionIndex = objReader.getHead("master");

            if (mostRecentVersionIndex != "NULL")
            {

                // TODO: Get the tree from the commit object
                Version versionObject = objReader.getVersion(mostRecentVersionIndex);

                if (versionObject.versionIndex != "error")
                {
                    // TODO: Traverse the tree and the current directory together
                    traverseTreeWithDir(filesystem::path(currentPath), versionObject.treeIndex, newFiles, deletedFiles, modifiedFiles, ignores);
                }
                else
                {
                    cout << "Could not open commit object" << endl;
                }
            }
            else
            {
                cout << "Could not open branch file" << endl;
            }
        }
    }

    void displayVersionInfo(string currentVersion) {

        if (currentVersion == "NULL") {
            cout << "Repository is new with no saved version.\n";
        }
        else {
            cout << "Current version: Version '" << currentVersion << "'.\n";
        }
    }

    void displayResult(const vector<string> &newFiles, const vector<string> &deletedFiles, const vector<string> &modifiedFiles)
    {

        int nSize = newFiles.size();
        int dSize = deletedFiles.size();
        int mSize = modifiedFiles.size();

        // HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

        if (nSize > 0)
        {
            cout << "NEW files:\n";
            // SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
            for (const string newFile : newFiles)
            {
                // printf("\tnew: " + newFile + "\n");

                cout << "\tnew: " << newFile << "\n";
            }
            // SetConsoleTextAttribute(hConsole, NORMAL_PRINT);
        }

        if (mSize > 0)
        {
            cout << "MODIFIED files:\n";
            // SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
            for (const string modifiedFile : modifiedFiles)
            {
                // printf("\t\x1b[31m modified: %s \x1b[0m\n", modifiedFile);
                cout << "\tmodified: " << modifiedFile << "\n";
            }
        }

        if (dSize > 0)
        {
            cout << "DELETED files:\n";
            // SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
            for (const string deletedFile : deletedFiles)
            {
                // printf("\t\x1b deleted: %s \x1b \n", deletedFile);
                cout << "\tdeleted: " << deletedFile << "\n";
            }
            // SetConsoleTextAttribute(hConsole, NORMAL_PRINT);
        }

        if (nSize + mSize + dSize == 0)
        {
            cout << "No changes detected since last save." << endl;
        }
    }

public:
    bool unsavedChangesExist()
    {
        // Prepare the 3 categories of files
        vector<string> newFiles;
        vector<string> deletedFiles;
        vector<string> modifiedFiles;

        // Prepare the ignores set
        set<string> ignores;
        objReader.getIgnores(ignores);

        // Meat and butter of the functionality
        getStatus(".\\", newFiles, deletedFiles, modifiedFiles, ignores);
        if (newFiles.size() + deletedFiles.size() + modifiedFiles.size() > 0)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    void execute()
    {
        // Prepare the 3 categories of files
        vector<string> newFiles;
        vector<string> deletedFiles;
        vector<string> modifiedFiles;

        // Prepare the ignores set
        set<string> ignores;
        objReader.getIgnores(ignores);

        // Meat and butter of the functionality
        getStatus(".\\", newFiles, deletedFiles, modifiedFiles, ignores);

        // Get current version
        string currentVersion = objReader.getHead("master");

        // Print out all untracked files
        displayVersionInfo(currentVersion);
        displayResult(newFiles, deletedFiles, modifiedFiles);
    }
};

#endif