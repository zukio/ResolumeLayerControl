#import <Foundation/Foundation.h>
#include "Platform.h"
#include "ServerSettings.h"
#include <cstdlib>
#include <stdexcept>
namespace control {
std::filesystem::path HostServerSettingsPath() {
    @autoreleasepool {
        NSString* name = [[[NSBundle mainBundle] executablePath] lastPathComponent];
        // Executable names require target-Mac validation; unknown hosts fail closed.
        if (![name isEqualToString:@"Arena"] && ![name isEqualToString:@"Resolume Arena"])
            throw std::runtime_error("unsupported Resolume host (Arena required)");
        NSURL* documents = [[[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask] firstObject];
        if (!documents) throw std::runtime_error("cannot locate Documents folder");
        return ServerSettingsPath(std::filesystem::path([[documents path] fileSystemRepresentation]));
    }
}
std::filesystem::path LogPathFromEnvironment() {
    const char* path = std::getenv("RLC_WS_LOG");
    return path && *path ? std::filesystem::u8path(path) : std::filesystem::path();
}
void DebugLog(const std::string& text) {
    @autoreleasepool { NSLog(@"%s", text.c_str()); }
}
std::string NewInstanceKey() {
    @autoreleasepool { return [[[NSUUID UUID] UUIDString] UTF8String]; }
}
}
