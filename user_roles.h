#ifndef USER_ROLES_H
#define USER_ROLES_H

#include <string>
#include <Arduino.h>

enum UserRole {
    ADMIN,      // Full control - can control LED, view logs, manage settings
    MODERATOR,  // Can control LED but no settings access
    VIEWER,     // Can only view LED status
    GUEST       // Limited view only
};

struct User {
    std::string username;
    std::string password;
    UserRole role;
};

// Predefined users with different roles
const User users[] = {
    {"admin", "admin123", ADMIN},
    {"moderator", "mod123", MODERATOR},
    {"viewer", "view123", VIEWER},
    {"guest", "guest123", GUEST}
};

const int NUM_USERS = sizeof(users) / sizeof(users[0]);
const int LED_PIN = 2;

// Permission structure
struct Permissions {
    bool canControlLED;
    bool canViewStatus;
    bool canViewLogs;
    bool canChangeSettings;
    bool canAccessAPI;
};

// Get permissions for a role - INLINE function
inline Permissions getPermissions(UserRole role) {
    Permissions perms = {false, false, false, false, false};
    
    switch(role) {
        case ADMIN:
            perms.canControlLED = true;
            perms.canViewStatus = true;
            perms.canViewLogs = true;
            perms.canChangeSettings = true;
            perms.canAccessAPI = true;
            break;
        case MODERATOR:
            perms.canControlLED = true;
            perms.canViewStatus = true;
            perms.canViewLogs = true;
            perms.canChangeSettings = false;
            perms.canAccessAPI = true;
            break;
        case VIEWER:
            perms.canControlLED = false;
            perms.canViewStatus = true;
            perms.canViewLogs = true;
            perms.canChangeSettings = false;
            perms.canAccessAPI = false;
            break;
        case GUEST:
            perms.canControlLED = false;
            perms.canViewStatus = true;
            perms.canViewLogs = false;
            perms.canChangeSettings = false;
            perms.canAccessAPI = false;
            break;
    }
    
    return perms;
}

// Get role name as string - INLINE function
inline const char* getRoleName(UserRole role) {
    switch(role) {
        case ADMIN: return "Admin";
        case MODERATOR: return "Moderator";
        case VIEWER: return "Viewer";
        case GUEST: return "Guest";
        default: return "Unknown";
    }
}

// Authenticate user - INLINE function
inline User* authenticateUser(String username, String password) {
    for (int i = 0; i < NUM_USERS; i++) {
        if (users[i].username == username.c_str() && 
            users[i].password == password.c_str()) {
            return (User*)&users[i];
        }
    }
    return nullptr;
}

// Legacy function declarations
bool controlLED(User user, bool turnOn);
bool viewLEDStatus(User user);

#endif