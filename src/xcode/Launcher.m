#import "Launcher.h"
#import "SDL2/SDL.h"
#import <crt_externs.h>

// If you make a MOD then please change this, the bundle indentifier, the file extensions (.ogz, .dmo), and the url registration.
#define kSAUERBRATEN @"sauerbraten"

@implementation Launcher

- (void)findPaths
{
    NSString *path = [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"Contents/gamedata"];
    NSFileManager *fm = [NSFileManager defaultManager];
    if ([fm fileExistsAtPath:path])
    {
        dataPath = [path retain];
    }
    else  // development setup
    {
        NSString *type = nil;
        NSString *paths[] = {
            [[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent], // relative to the binary
            [NSString stringWithUTF8String:__FILE__]  // relative to the source code - xcode 4+
        };
        for(size_t i = 0; i < sizeof(paths)/sizeof(NSString*); i++)
        {
            path = paths[i];
            // search up the folder till find a folder containing packages, or a game application containing packages
            while ([path length] > 1)
            {
                path = [path stringByDeletingLastPathComponent];
                
                NSString *probe = [path stringByAppendingPathComponent:[NSString stringWithFormat:@"%@.app/Contents/gamedata", kSAUERBRATEN]];
                if ([fm fileExistsAtPath:[probe stringByAppendingPathComponent:@"packages"]])
                {
                    dataPath = [probe retain];
                    type = @"recompiled";
                    break;
                }
                else if ([fm fileExistsAtPath:[path stringByAppendingPathComponent:@"packages"]])
                {
                    dataPath = [path retain];
                    type = @"svn";
                    break;
                }
            }
            if(type) break;
        }
        NSLog(@"type=%@", type);
    }
    // userpath: directory where user files are kept - typically /Users/<name>/Application Support/sauerbraten
    path = nil;
    NSArray *supports = [fm URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask];
    if ([supports count])
    {
        path = [[supports objectAtIndex:0] path];
        path = [path stringByAppendingPathComponent:kSAUERBRATEN];
        if (![fm fileExistsAtPath:path]) [fm createDirectoryAtPath:path withIntermediateDirectories:YES attributes:nil error:nil]; // ensure it exists
    }
    userPath = [path retain];    
}

- (void)awakeFromNib
{
    [self findPaths];
        
    [NSApp setDelegate:self]; // so can catch the double-click, dropped files, termination
    [[NSAppleEventManager sharedAppleEventManager] setEventHandler:self andSelector:@selector(getUrl:withReplyEvent:) forEventClass:kInternetEventClass andEventID:kAEGetURL];
}

- (void)setCommand:(NSString*)c
{
    [command release];
    command = [c retain];
}

- (NSArray*)launchArgs
{
    NSMutableArray *args = [NSMutableArray array];
    [args addObject:[NSString stringWithFormat:@"-q%@", userPath]];     

    char **argv = *_NSGetArgv();
    int argc = *_NSGetArgc();
    BOOL launcher = YES;
    for(int i = 0; i < argc; i++) if(strcmp(argv[i], "-nolauncher")==0) { launcher = NO; break; }
    if(launcher)
    {
         if(command) [args addObject:command];
    }
    else
    {
        // copy all args except program name and "-nolauncher" arg
        for(int i = 1; i < argc; i++) 
        {
            if(strcmp(argv[i], "-nolauncher")==0) continue;
            [args addObject:[NSString stringWithUTF8String:argv[i]]];
        }
        
    }
    return args;
}

- (void)launchGame:(NSArray *)args
{
    const int argc = [args count] + 1;
    const char **argv = (const char**)malloc(sizeof(char*)*(argc + 1)); // {path, <args>, NULL};
    argv[0] = [[[NSBundle mainBundle] executablePath] fileSystemRepresentation];
    argv[argc] = NULL;
    for (int i = 1; i < argc; i++) argv[i] = [[args objectAtIndex:i-1] UTF8String];
    
    // call back into C/C++ world
    if(dataPath) chdir([dataPath fileSystemRepresentation]);
    SDL_main(argc, (char**)argv);    
    // won't reach here as the C/C++ code calls fatal/exit
}

#pragma mark -
#pragma mark application delegate

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    [self launchGame:[self launchArgs]];
}

-(BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication
{
    return NO;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    return NSTerminateCancel; // let the game eventually terminate itself via exit() rather than the app terminating itself now
}

#pragma mark -
#pragma mark file opening

- (NSString*)roleForExtension:(NSString*)fileExtension
{
    // from the plist determine that dmo->Viewer, and ogz->Editor
    for(NSDictionary *type in [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleDocumentTypes"])
    {
        NSString *role = [type objectForKey:@"CFBundleTypeRole"];
        NSArray *exts = [type objectForKey:@"CFBundleTypeExtensions"];
        if([exts indexOfObject:fileExtension] != NSNotFound) return role;
    }
    return nil;
}

// plist registers 'ogz' and 'dmo' as doc types
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
    NSString *role = [self roleForExtension:[filename pathExtension]];
    if (!role) return NO;
    BOOL demo = [role isEqualToString:@"Viewer"];
    filename = [filename stringByDeletingPathExtension]; // chop off extension
    for (int i = 0; i < 2; i++)
    {
        NSString *pkg = (i == 0) ? dataPath : userPath;
        if (!demo) pkg = [pkg stringByAppendingPathComponent:@"packages"];
        if ([filename hasPrefix:pkg])
        {
            [self setCommand:(demo ? [NSString stringWithFormat:@"-xdemo \"%@\"", filename] : [NSString stringWithFormat:@"-l%@", filename])];
            return YES;
        }
    }
    return NO;
}

#pragma mark -
#pragma mark url opening

// plist registers 'sauerbraten' as a url scheme
- (void)getUrl:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
    NSURL *url = [NSURL URLWithString:[[event paramDescriptorForKeyword:keyDirectObject] stringValue]];
    if(url) [self setCommand:[NSString stringWithFormat:@"-xconnect %@ %ld", [url host], (long)[[url port] intValue]]];
}

@end
