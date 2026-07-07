#include <iostream>
#include <string>
#include <vector>
#include <Windows.h>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <openssl/sha.h>
#include <unordered_map>
#include <iomanip>
#include <random>
#include <sstream>
#include <openssl/evp.h>

using namespace std;

class FileManager {
public:
    // 读取文件内容
    static vector<string> readConfig(const string& filename) {
        if (!fileExists(filename)) {
            createEmptyFile(filename);
            return {};
        }

        ifstream file(filename);
        if (!file.is_open()) {
            throw runtime_error("Failed to open file: " + filename);
        }

        vector<string> lines;
        string line;
        while (getline(file, line)) {
            if (!line.empty()) {
                lines.push_back(line);
            }
        }
        return lines;
    }

    // 写入文件内容
    static void writeConfig(const string& filename, const vector<string>& contents, bool append = false) {
        ios_base::openmode mode = ios::out;
        if (append) {
            mode |= ios::app;
        }

        ofstream file(filename, mode);
        if (!file.is_open()) {
            throw runtime_error("Failed to write to file: " + filename);
        }

        for (const auto& line : contents) {
            file << line << '\n';
        }
    }

    // 生成随机盐值（增强安全性）
    static string generateSalt() {
        constexpr int SALT_LENGTH = 32;
        static const char alphanum[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";

        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dist(0, sizeof(alphanum) - 2);

        string salt;
        salt.reserve(SALT_LENGTH);
        for (int i = 0; i < SALT_LENGTH; ++i) {
            salt += alphanum[dist(gen)];
        }
        return salt;
    }

    // 使用正确的SHA256函数
    static string hashPassword(const string& password, const string& salt) {
        string saltedPassword = password + salt;
        unsigned char hash[SHA256_DIGEST_LENGTH];

        // 使用EVP接口替代已弃用的函数
        EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
        if (mdctx == nullptr) {
            throw runtime_error("Failed to create EVP context");
        }

        if (1 != EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr)) {
            EVP_MD_CTX_free(mdctx);
            throw runtime_error("Digest init failed");
        }

        if (1 != EVP_DigestUpdate(mdctx, saltedPassword.c_str(), saltedPassword.size())) {
            EVP_MD_CTX_free(mdctx);
            throw runtime_error("Digest update failed");
        }

        unsigned int len = 0;
        if (1 != EVP_DigestFinal_ex(mdctx, hash, &len)) {
            EVP_MD_CTX_free(mdctx);
            throw runtime_error("Digest final failed");
        }

        EVP_MD_CTX_free(mdctx);

        stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << hex << setw(2) << setfill('0') << static_cast<int>(hash[i]);
        }
        return ss.str();
    }

private:
    // 改进文件存在检查
    static bool fileExists(const string& filename) {
        ifstream file(filename);
        return file.good() && file.is_open();
    }

    // 创建空文件（添加异常处理）
    static void createEmptyFile(const string& filename) {
        ofstream file(filename);
        if (!file) {
            throw runtime_error("Failed to create config file: " + filename);
        }
        file.close();  // 显式关闭文件
    }
};



class UserManager {
public:
    UserManager() : fileManager(make_unique<FileManager>()) {}

    // 创建用户（增加输入验证）
    void createUser() {
        system("cls");
        string username, password;

        cout << "Please enter user name (3-20 characters, a-z0-9_):\n->";
        while (true) {
            cin >> username;
            if (isValidUsername(username)) break;
            cout << "Invalid username format. Try again:\n->";
        }

        if (isUserExists(username)) {
            cout << "User name is already in use.\n";
            return;
        }

        cout << "Please enter user password (min 8 characters):\n->";
        while (true) {
            cin >> password;
            if (password.length() >= 8) break;
            cout << "Password too short. Minimum 8 characters:\n->";
        }

        string salt = FileManager::generateSalt();
        string hashedPassword = FileManager::hashPassword(password, salt);

        FileManager::writeConfig("USER.cfg", { username }, true);
        FileManager::writeConfig("SALT.cfg", { salt }, true);
        FileManager::writeConfig("PASSWORD.cfg", { hashedPassword }, true);
        if (isUserExists(username, true)) cout << "User created successfully!\n";
        Sleep(2000);
    }

    // 用户名格式验证
    bool isValidUsername(const string& username) {
        constexpr size_t MIN_LENGTH = 3;
        constexpr size_t MAX_LENGTH = 20;
        if (username.length() < MIN_LENGTH || username.length() > MAX_LENGTH)
            return false;

        return username.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") == string::npos;
    }

    

    bool ManageUser(const string& username) {
        system("cls");
        if (!isUserExists(username)) {
            cout << "User does not exist.\n";
            return false;
        }
        // 管理用户逻辑（例如重置密码、删除用户等）
        while (true) {
            cout << "Managing user: " << username << "\n";
            cout << "Services: 0 - Rest the password, 1 - Delete the account, 2 - Exit the service\n->";
            string choice;
            cin >> choice;

            if (choice == "0") {
				return RestPassword(username);
            }
            else if (choice == "1") {
                return DeleteUser(username);
            }
            else if (choice == "2") {
                return false;
            }
            else {
                cout << "Invalid choice. Please try again.\n";
                Sleep(1000);
				system("cls");
            }
            
        }
	}

    // 用户存在检查（使用哈希表优化）
    bool isUserExists(const string& username,bool changed=false) {
        static unordered_map<string, bool> userCache;
        static bool cacheLoaded = false;
        

        if (!cacheLoaded || changed) {
            auto users = FileManager::readConfig("USER.cfg");
            for (const auto& user : users) {
                userCache[user] = true;
            }
            if(cacheLoaded) cacheLoaded = true;
			if (changed) changed = false;
        }

        return userCache.find(username) != userCache.end();
    }
    vector <string> getUsers() {
        return getAllUsers();
	}
    // 用户认证（增加尝试次数限制）
    bool authenticateUser(const string& username, const string& password, int maxAttempts = 3) {
        static unordered_map<string, int> attemptCount;

        if (attemptCount[username] >= maxAttempts) {
            cout << "Account locked due to too many failed attempts.\n";
            return false;
        }

        auto users = FileManager::readConfig("USER.cfg");
        auto salts = FileManager::readConfig("SALT.cfg");
        auto passwords = FileManager::readConfig("PASSWORD.cfg");

        for (size_t i = 0; i < users.size(); ++i) {
            if (users[i] == username) {
                string hashedPassword = FileManager::hashPassword(password, salts[i]);
                if (hashedPassword == passwords[i]) {
                    attemptCount[username] = 0;
                    return true;
                }
            }
        }

        attemptCount[username]++;
        return false;
    }


private:
    unique_ptr<FileManager> fileManager;
    vector <string> getAllUsers() {
        return FileManager::readConfig("USER.cfg");
    }
    bool DeleteUser(const string& username) {
        if (!isUserExists(username)) {
            cout << "User does not exist.\n";
			Sleep(1000);
            return false;
        }
        auto users = FileManager::readConfig("USER.cfg");
        auto salts = FileManager::readConfig("SALT.cfg");
        auto passwords = FileManager::readConfig("PASSWORD.cfg");
        auto it = find(users.begin(), users.end(), username);
        if (it != users.end()) {
            size_t index = distance(users.begin(), it);
            users.erase(it);
            salts.erase(salts.begin() + index);
            passwords.erase(passwords.begin() + index);
            FileManager::writeConfig("USER.cfg", users);
            FileManager::writeConfig("SALT.cfg", salts);
            FileManager::writeConfig("PASSWORD.cfg", passwords);
			if (!isUserExists(username, true)) cout << "User deleted successfully!\n";
            Sleep(1000);
            return true;
        }
        return false;
    }
	bool RestPassword(const string& username) {
        if (!isUserExists(username)) {
            cout << "User does not exist.\n";
            return false;
        }
        cout << "Please enter old password->\n";
        string psd;
        cin >> psd;
        if(authenticateUser(username, psd))
        {
            string newPassword;
            cout << "Please enter new password (min 8 characters):\n->";
            while (true) {
                cin >> newPassword;
                if (newPassword.length() >= 8) break;
                cout << "Password too short. Minimum 8 characters:\n->";
            }
            auto users = FileManager::readConfig("USER.cfg");
            auto salts = FileManager::readConfig("SALT.cfg");
            auto passwords = FileManager::readConfig("PASSWORD.cfg");
            auto it = find(users.begin(), users.end(), username);
            if (it != users.end()) {
                size_t index = distance(users.begin(), it);
                string salt = FileManager::generateSalt();
                string hashedPassword = FileManager::hashPassword(newPassword, salt);
                salts[index] = salt;
                passwords[index] = hashedPassword;
                FileManager::writeConfig("SALT.cfg", salts);
                FileManager::writeConfig("PASSWORD.cfg", passwords);
                cout << "Password reset successfully!\n";
                Sleep(1000);
                return true;
            }
		}
		else cout << "Wrong password!\n";
        return false;
    }
};


void exitProgram() {
    system("cls");
    cout << "\aThanks for using!";
    Sleep(2000);
    exit(0);
}
class Menus {
public:
    void adminMenu(UserManager& userManager) {
        while (true) {
            system("cls");
            cout << "Welcome Admin\nServices: 0 - Create an account, 1 - Manage an account, 2 - Login out\n->";
            string choice;
            cin >> choice;

            if (choice == "0") {
                userManager.createUser();
                Sleep(1000);
            }
            else if (choice == "1") {
                cout << "Please enter the username you want to manage:\n->";
                for (auto i : userManager.getUsers()) {
                    cout << i << "\n";
                }
                string username;
                cin >> username;
                if (userManager.ManageUser(username)) cout << "\aSuccess!";
                Sleep(1000);
            }
            else if (choice == "2") {
                break;
            }
            else {
                cout << "Invalid choice. Please try again.\n";
                Sleep(1000);
            }
        }
    }
    void userMenu(UserManager& userManager,const string& username) {
        string choice;
        while (true) {
            system("cls");
            cout << "Welcome, " << username << "!\nServices: 0 - Manage user, 1 - Manage data , 2 - Login out\n->";
            cin >> choice;
            if (choice == "0") {
                if (userManager.isUserExists(username)) { userManager.ManageUser(username); }
                else { break; };
                Sleep(1000);
            }
			else if (choice == "1")
            {
				system("cls");
                cout << "WIP";
				Sleep(1000);
            }
            else if (choice == "2") {
                break;
            }
            else {
                cout << "Invalid choice. Please try again.\n";
                Sleep(1000);
            }
        }
	}

};
int main()
{
    try {
        UserManager userManager;
        Menus m;
        string username, password;
        bool 超管;
        超管 = false;
        while (true)
        {
            cout << "Welcome!\nPlease enter your username. Enter /exit to exit\n->";
            cin >> username;

            if (username == "/exit") {
                exitProgram();
            }

            cout << "Please enter your password\n->";
            cin >> password;
            if (password == "/exit") {
                exitProgram();
            }
            if (username == "Admin" && password == "Super_Admin1234" || 超管) {
                m.adminMenu(userManager);

            }
            else if (userManager.authenticateUser(username, password)) {
                cout << "Welcome, " << username << "!\n";
                m.userMenu(userManager, username);
				username.clear();
				password.clear();
            }
            else {
                cout << "Invalid username or password.\n";
            }
			system("cls");
        }
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
