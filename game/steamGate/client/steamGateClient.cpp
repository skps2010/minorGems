

#include "minorGems/util/SettingsManager.h"
#include "minorGems/util/stringUtils.h"
#include "minorGems/network/web/WebClient.h"
#include "minorGems/formats/encodingUtils.h"
#include "minorGems/util/log/AppLog.h"
#include "minorGems/util/log/FileLog.h"
#include "minorGems/system/Thread.h"
#include "minorGems/system/Time.h"
#include "minorGems/io/file/File.h"
#include "minorGems/util/SimpleVector.h"


#include "minorGems/crypto/cryptoRandom.h"
#include "minorGems/crypto/keyExchange/curve25519.h"

#define STEAM_API_NON_VERSIONED_INTERFACES 1

#include "steam/steam_api.h"
//#include "openSteamworks/Steamclient.h"


/////////////////////
// settings:
static const char *steamGateServerURL = 
"http://onehouronelife.com/steamGate/server.php";

#define linuxLaunchTarget "./OneLifeApp"
#define macLaunchTarget "OneLife.app"
#define winLaunchTarget "OneLife.exe"

#define gameName "One Hour One Life"

// end settings
/////////////////////



#ifdef __mac__

#include <unistd.h>
#include <stdarg.h>
#include <sys/wait.h>

static void launchGame() {
    AppLog::info( "Launching game." );
    int forkValue = fork();

    if( forkValue == 0 ) {
        // we're in child process, so exec command
        char *arguments[3] = { "open", (char*)macLaunchTarget, NULL };
                
        execvp( "open", arguments );

        // we'll never return from this call
        }
    else {
        // parent
        AppLog::infoF( "Waiting for child game process %d to exit.",
                       forkValue );
        double startTime = Time::getCurrentTime();
        int returnStatus;    
        waitpid( forkValue, &returnStatus, 0 );

        AppLog::infoF( "Child game process ran for %f minutes before exiting.",
                       ( Time::getCurrentTime() - startTime ) / 60 );
        }
    }


static void showMessage( const char *inTitle, const char *inMessage,
                         char inError = false ) {
    const char *iconName = "note";
    if( inError ) {
        iconName = "stop";
        }

    const char *commandFormat =
        "osascript -e 'tell app \"System Events\" to activate' "
        "-e 'tell app \"System Events\" to display dialog \"%s\" "
        "with title \"%s\" buttons \"Ok\" "
        "with icon %s default button \"Ok\"' "
        "-e 'tell app \"System Events\" to quit' ";
    
    char *command = autoSprintf( commandFormat, inMessage, inTitle, 
                                 iconName );
    
    FILE *osascriptPipe = popen( command, "r" );
    
    delete [] command;
    
    if( osascriptPipe == NULL ) {
        AppLog::error( 
            "Failed to open pipe to osascript for displaying GUI messages." );
        }
    else {
        pclose( osascriptPipe );
        }
    }



#elif defined(LINUX)

#include <unistd.h>
#include <stdarg.h>
#include <sys/wait.h>

static void launchGame() {
    AppLog::info( "Launching game" );
    int forkValue = fork();

    if( forkValue == 0 ) {
        // we're in child process, so exec command
        char *arguments[2] = { (char*)linuxLaunchTarget, NULL };
                
        execvp( linuxLaunchTarget, arguments );

        // we'll never return from this call
        }
    else {
        // parent
        AppLog::infoF( "Waiting for child game process %d to exit.",
                       forkValue );
        double startTime = Time::getCurrentTime();
        int returnStatus;    
        waitpid( forkValue, &returnStatus, 0 );

        AppLog::infoF( "Child game process ran for %f minutes before exiting.",
                       ( Time::getCurrentTime() - startTime ) / 60 );
        }
    }



static void showMessage( const char *inTitle, const char *inMessage,
                         char inError = false ) {
    const char *zenCommand = "--info";
    if( inError ) {
        zenCommand = "--error";
        }

    char *command = autoSprintf( "zenity %s --text='%s' --title='%s'",
                                 zenCommand, inMessage, inTitle );

    FILE *zentityPipe = popen( command, "r" );
    
    delete [] command;

    if( zentityPipe == NULL ) {
        AppLog::error( 
            "Failed to open pipe to zenity for displaying GUI messages." );
        }
    else {
        pclose( zentityPipe );
        }
    }


#elif defined(WIN_32)

#include <windows.h>
#include <process.h>

static void launchGame() {
    AppLog::info( "Launching game" );
    char *arguments[2] = { (char*)winLaunchTarget, NULL };
    
    AppLog::infoF( "Waiting for child game process to exit after launching." );
    double startTime = Time::getCurrentTime();
    
    _spawnvp( _P_WAIT, winLaunchTarget, arguments );
    
    AppLog::infoF( "Child game process ran for %f minutes before exiting.",
                   ( Time::getCurrentTime() - startTime ) / 60 );
    }


#include <stdlib.h>

static void showMessage( const char *inTitle, const char *inMessage,
                         char inError = false ) {
    UINT uType = MB_OK | MB_TOPMOST;
    
    if( inError ) {
        uType |= MB_ICONERROR;
        }
    else {
        uType |= MB_ICONINFORMATION;    
        }
    
    wchar_t *wideTitle = new wchar_t[ strlen( inTitle ) * 2 + 2 ];
    mbstowcs( wideTitle, inTitle, strlen( inTitle ) + 1 );
    
    wchar_t *wideMessage = new wchar_t[ strlen( inMessage ) * 2 + 2 ];
    mbstowcs( wideMessage, inMessage, strlen( inMessage ) + 1 );
    
    MessageBox( NULL, wideMessage, wideTitle, uType );

    delete [] wideTitle;
    delete [] wideMessage;
    }


int main();


int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPSTR lpCmdLine, int nShowCMD ) {
    
    return main();
    }

#endif





static char authTicketCallbackCalled = false;
static char authTicketCallbackError = false;



// a class that does nothing other than register itself as a Steam
// callback listener when it is constructed
class AuthTicketListener {
        
    public:
        
        AuthTicketListener() 
                // initializing this member object is what actually
                // registers our callback
                : mAuthSessionTicketResponse( 
                    this, 
                    &AuthTicketListener::OnAuthSessionTicketResponse ) {
            AppLog::info( "AuthTicketListener instantiated" );
            }

        
        // callback when our AuthSessionTicket is ready
        // this macro declares our callback function for us AND
        // defines our member object
        STEAM_CALLBACK( 
            // class that the callback function is in
            AuthTicketListener, 
            // name of callback function
            OnAuthSessionTicketResponse,
            // type of callback
            GetAuthSessionTicketResponse_t, 
            // name of the member object       
            mAuthSessionTicketResponse );


    };



void AuthTicketListener::OnAuthSessionTicketResponse( 
    GetAuthSessionTicketResponse_t *inCallback ) {
    
    AppLog::info( "AuthTicketListener callback called" );

    authTicketCallbackCalled = true;
            
    if ( inCallback->m_eResult != k_EResultOK ) {
        authTicketCallbackError = true;
        }
    }        




static void showTicketCreationError() {
    showMessage( gameName ":  Error",
                 "Could not create an account for you on the game server.",
                 true );
    }



void processMods();



int main() {

    AppLog::setLog( new FileLog( "log_steamGate.txt" ) );
    AppLog::setLoggingLevel( Log::DETAIL_LEVEL );


    // before we even check for login info, see if we've been tasked
    // with marking the app as dirty
    FILE *f = fopen( "steamGateForceUpdate.txt", "r" );
    if( f != NULL ) {
        AppLog::info( "steamGateForceUpdate.txt file exists." );

        int val = 0;
        
        fscanf( f, "%d", &val );
        fclose( f );


        if( val == 1 ) {
            AppLog::info( "steamGateForceUpdate.txt file contains '1' flag." );

            // we've been signaled to mark app as dirty
            
            if( ! SteamAPI_Init() ) {
                showMessage( gameName ":  Error",
                             "Failed to connect to Steam.",
                             true );
                AppLog::error( "Could not init Steam API." );
                return 0;
                }

            AppLog::info( "Calling MarkContentCorrupt." );

            // only check for missing files
            // not sure what this param does if a new depot is available
            SteamApps()->MarkContentCorrupt( true );
            
            // done, mark so we skip this next time
            f = fopen( "steamGateForceUpdate.txt", "w" );
            if( f != NULL ) {
                fprintf( f, "0" );
                fclose( f );
                }
            
            AppLog::info( "Done forcing update, exiting." );
            SteamAPI_Shutdown();
            return 0;
            }
        }


    char *accountKey = SettingsManager::getStringSetting( "accountKey" );
    char *email = SettingsManager::getStringSetting( "email" );


    if( accountKey != NULL && 
        email != NULL &&
        strcmp( accountKey, "" ) != 0 &&
        strcmp( email, "" ) != 0 ) {

        delete [] accountKey;
        delete [] email;
        
        AppLog::info( "We already have saved login info.  Launching game." );

        processMods();

        launchGame();
        AppLog::info( "Exiting." );
        return 0;
        }

    
    if( accountKey != NULL ) {
        delete [] accountKey;
        }
     
    if( email != NULL ) {
        delete [] email;
        }
    
    showMessage( gameName ":  First Launch",
                 "The game will try to create a server-side account for"
                 " you through Steam.\n\nThis may take a few moments." );
    
    AppLog::info( "No login info found.  "
                  "Executing first-login protocol with server." );

    
    if( ! SteamAPI_Init() ) {
        showMessage( gameName ":  Error",
                     "Failed to connect to Steam.",
                     true );
        AppLog::error( "Could not init Steam API." );
        return 0;
        }
    

    CSteamID id = SteamUser()->GetSteamID();
    
    uint64 rawID = id.ConvertToUint64();

    
    
    AppLog::infoF( "Steam ID %lli", rawID );


    unsigned int appID = SteamUtils()->GetAppID();

    AppLog::infoF( "App ID %d", appID );
    

    char loggedOn = SteamUser()->BLoggedOn();
    
    AppLog::infoF( "Logged onto steam: %d", loggedOn );

    char behindNAT = SteamUser()->BIsBehindNAT();
    
    AppLog::infoF( "Behind NAT: %d", behindNAT );

    /*
    EUserHasLicenseForAppResult doesNotOwnApp =
        SteamUser()->UserHasLicenseForApp( id, appID );
    
    AppLog::infoF( "doesNotOwnApp = %d", doesNotOwnApp );
    */

    // construct a listener to add it to Steam's listener pool
    AuthTicketListener *listener = new AuthTicketListener();

    unsigned char authTicketData[2048];
    uint32 authTicketSize = 0;
    HAuthTicket ticketHandle =
        SteamUser()->GetAuthSessionTicket( 
            authTicketData, sizeof(authTicketData), 
            &authTicketSize );
        

    AppLog::infoF( "GetAuthSessionTicket returned %d bytes, handle %d",
                   authTicketSize, ticketHandle );

    if( authTicketSize == 0 ) {
        
        showMessage( gameName ":  Error",
                     "Could not get an Authentication Ticket from Steam.",
                     true );
        AppLog::error( "GetAuthSessionTicket returned 0 length." );
        
        SteamAPI_Shutdown();
        return 0;
        }
    

    AppLog::info( "Waiting for Steam GetAuthSessionTicket callback..." );
    
    double startTime = Time::getCurrentTime();
    double maxTime = 20;
    
    while( ! authTicketCallbackCalled ) {
        if( Time::getCurrentTime() - startTime > maxTime ) {
            showMessage( gameName ":  Error",
                         "Timed out waiting for "
                         "Authentication Ticket validation from Steam.",
                         true );
            AppLog::error( "Timed out." );
            SteamAPI_Shutdown();
            return 0;
            }
        
        Thread::staticSleep( 100 );
        SteamAPI_RunCallbacks();
        }

    AppLog::info( "...got callback." );

    // de-register listener
    delete listener;
    
    if( authTicketCallbackError ) {
        showMessage( gameName ":  Error",
                     "Could not validate the Steam Authentication Ticket.",
                     true );
        AppLog::error( "GetAuthSessionTicket callback returned error." );
        SteamAPI_Shutdown();
        return 0;
        }
    

    char *authTicketHex = hexEncode( authTicketData, authTicketSize );
    
    AppLog::infoF( "Auth ticket data:  %s", authTicketHex );


    


    
    unsigned char ourPubKey[32];
    unsigned char ourSecretKey[32];
    
    char gotSecret = 
        getCryptoRandomBytes( ourSecretKey, 32 );
    
    if( ! gotSecret ) {
        showMessage( gameName ":  Error",
                     "Could not get random data to generate an "
                     "encryption key.",
                     true );
        AppLog::error( "Failed to get secure random bytes for "
                       "key generation." );
        SteamAPI_Shutdown();
        return 0;
        }
    
    
    curve25519_genPublicKey( ourPubKey, ourSecretKey );
    

    char *ourPubKeyHex = hexEncode( ourPubKey, 32 );
    
    char *webRequest = 
        autoSprintf( 
            "%s?action=get_account"
            "&auth_session_ticket=%s"
            "&client_public_key=%s",
            steamGateServerURL,
            authTicketHex,
            ourPubKeyHex );
            
    delete [] ourPubKeyHex;
    delete [] authTicketHex;

    AppLog::infoF( "Web request to URL: %s", webRequest );
    
    //printf( "Waiting....\n" );
    //int read;
    //scanf( "%d", &read );
    
    int resultLength;
    char *webResult = WebClient::getWebPage( webRequest, &resultLength );
         
    delete [] webRequest;
    

    if( webResult == NULL ) {
        showTicketCreationError();
        AppLog::error( "Failed to get response from server." );
        
        SteamAPI_Shutdown();
        return 0;
        }
    

    SimpleVector<char *> *tokens = tokenizeString( webResult );
    
    
    
    if( tokens->size() != 3 || 
        strlen( *( tokens->getElement( 0 ) ) ) != 64 ) {
        showTicketCreationError();
        AppLog::errorF( "Unexpected server response:  %s",
                        webResult );
        
        delete [] webResult;
        for( int i=0; i<tokens->size(); i++ ) {
            delete [] *( tokens->getElement(i) );
            }
        delete tokens;
        
        SteamAPI_Shutdown();
        return 0;
        }


    char *serverPublicKeyHex = *( tokens->getElement( 0 ) );
    email = *( tokens->getElement( 1 ) );
    char *encryptedTicketHex = *( tokens->getElement( 2 ) );


    unsigned char *serverPublicKey = hexDecode( serverPublicKeyHex );
    delete [] serverPublicKeyHex;
    
    if( serverPublicKey == NULL ) {
        showTicketCreationError();
        AppLog::errorF( "Unexpected server response:  %s",
                        webResult );
        
        delete [] email;
        delete [] encryptedTicketHex;
        delete [] webResult;
        delete tokens;
        
        SteamAPI_Shutdown();
        return 0;
        }
    
    
    unsigned char sharedSecretKey[32];

    curve25519_genSharedSecretKey( sharedSecretKey, ourSecretKey, 
                                   serverPublicKey );
    delete [] serverPublicKey;
    

    int numTicketBytes = strlen( encryptedTicketHex ) / 2;
    unsigned char *encryptedTicket = hexDecode( encryptedTicketHex );
    delete [] encryptedTicketHex;
    
    if( encryptedTicket == NULL ) {
        showTicketCreationError();
        AppLog::errorF( "Unexpected server response:  %s",
                        webResult );
    
        delete [] email;
        delete [] webResult;
        delete tokens;
        
        SteamAPI_Shutdown();
        return 0;
        }
    
    char *plaintextTicket = new char[ numTicketBytes + 1 ];
    
    for( int i=0; i<numTicketBytes; i++ ) {
        plaintextTicket[i] = encryptedTicket[i] ^ sharedSecretKey[i];
        }

    plaintextTicket[ numTicketBytes ] = '\0';
    delete [] encryptedTicket;


    AppLog::infoF( "Decrypted ticket as:  %s", plaintextTicket );
    
    SettingsManager::setSetting( "email", email );
    SettingsManager::setSetting( "accountKey", plaintextTicket );
    
    
    delete [] plaintextTicket;
    delete [] email;
    delete [] webResult;
    delete tokens;
    
    SteamAPI_Shutdown();

    showMessage( gameName ":  First Launch",
                 "Your server account is set up.\n\n"
                 "The game will launch now." );

    launchGame();
    
    AppLog::info( "Exiting." );
    return 0;
    }



void deallocateFileVector( SimpleVector<File*> *inVector ) {
    for( int i=0; i< inVector->size(); i++ ) {
        delete inVector->getElementDirect( i );
        }
    inVector->deleteAll();
    }



File *findFileInVector( SimpleVector<File*> *inVector, char *inName ) {
    for( int i=0; i< inVector->size(); i++ ) {
        File *f = inVector->getElementDirect( i );
        
        char *name = f->getFileName();
        
        if( strcmp( inName, name ) == 0 ) {
            return f;
            }
        }
    return NULL;
    }




typedef struct ModToUpload {
    File *oxzFile;
    File *jpgFile;
    } ModToUpload;




// Example class for receiving a call result
class CGameManager { 
    public: 
        void GetNumberOfCurrentPlayers();
    private:
        void OnGetNumberOfCurrentPlayers( NumberOfCurrentPlayers_t *pCallback,
                                          bool bIOFailure ); 
        CCallResult< CGameManager, NumberOfCurrentPlayers_t >
            m_NumberOfCurrentPlayersCallResult;
    };

// Make the asynchronous request to receive the number of current players.
void CGameManager::GetNumberOfCurrentPlayers() { 
    printf( "Getting Number of Current Players\n" );
    SteamAPICall_t hSteamAPICall =
        SteamUserStats()->GetNumberOfCurrentPlayers();
    
    m_NumberOfCurrentPlayersCallResult.Set( 
        hSteamAPICall, 
        this, 
        &CGameManager::OnGetNumberOfCurrentPlayers );
    }

// Called when SteamUserStats()->GetNumberOfCurrentPlayers() returns
// asynchronously, after a call to SteamAPI_RunCallbacks().
void CGameManager::OnGetNumberOfCurrentPlayers(
    NumberOfCurrentPlayers_t *pCallback, bool bIOFailure ) {
    
    if ( bIOFailure || !pCallback->m_bSuccess ) {
        printf( "NumberOfCurrentPlayers_t failed!\n" ); 
        return; 
        }
    printf( "Number of players currently playing: %d\n", 
            pCallback->m_cPlayers );
    }






void processModUploads() {
    File steamModUploadsDir( NULL, "steamModUploads" );
    
    if( ! steamModUploadsDir.exists() || ! steamModUploadsDir.isDirectory() ) {
        AppLog::info( "steamModUploads directory not found." );
        
        return;
        }
    SimpleVector<File *> oxzFiles;
    
    SimpleVector<File *> steamTxtFiles;
    
    SimpleVector<File *> jpgFiles;
    
    int numChildFiles;
    File **childFiles = steamModUploadsDir.getChildFiles( &numChildFiles );
    
    for( int i=0; i<numChildFiles; i++ ) {
        File *f = childFiles[i];
        
        char *name = f->getFileName();
        
        char nameLen = strlen( name );

        if( nameLen > 4 && 
            strcmp( &( name[ nameLen - 4 ] ), ".oxz" ) == 0 ) {
            // ends with .oxz
            oxzFiles.push_back( f );
            }
        else if( nameLen > 4 && 
                 strcmp( &( name[ nameLen - 4 ] ), ".jpg" ) == 0 ) {
            // ends with .oxz
            jpgFiles.push_back( f );
            }
        else if( nameLen > 10 && 
                 strcmp( &( name[ nameLen - 10 ] ), "_steam.txt" ) == 0 ) {
            // a companion file
            steamTxtFiles.push_back( f );
            }
        else {
            delete f;
            }
        
        delete [] name;
        }
    delete [] childFiles;



    SimpleVector<ModToUpload> uploadList;
    

    for( int i=0; i< oxzFiles.size(); i++ ) {
        File *f = oxzFiles.getElementDirect( i );
        
        char *name = f->getFileName();
        
        // strip extension
        name[ strlen(name) - 4 ] = '\0';
        
        char *steamTxtName = autoSprintf( "%s_steam.txt", name );
        char *jpgName = autoSprintf( "%s.jpg", name );
        
        File *jpgFile = findFileInVector( &jpgFiles, jpgName );

        if( jpgFile != NULL ) {

            File *steamFile = findFileInVector( &steamTxtFiles, steamTxtName );
            
            if( steamFile == NULL ) {
                // never been uploaded before, upload now
                ModToUpload m = { f, jpgFile };
                
                uploadList.push_back( m );
                
                char *message = 
                    autoSprintf( 
                        "Uploading new mod %s to Steam Workshop.", name );
            
                showMessage( gameName ":  Steam Workshop",
                             message,
                             true );
                delete [] message;
                }
            else {
                // steam companion file exists for this mod
                
                // check dates... has mod been updated locally since last
                // upload?
                
                timeSec_t oxzTime = f->getModificationTime();
                timeSec_t steamTxtTime = steamFile->getModificationTime();
                
                if( oxzTime > steamTxtTime ) {
                    ModToUpload m = { f, jpgFile };
                
                    uploadList.push_back( m );
                
                    char *message = 
                        autoSprintf( 
                            "Mod %s has changed, uploading latest version "
                            "to Steam Workshop.", name );
                
                    showMessage( gameName ":  Steam Workshop",
                                 message,
                                 true );
                    delete [] message;
                    }
                }
            }
        else {
            char *message = 
                autoSprintf( 
                    "Preview file %s missing for mod in "
                    "steamUModUploads folder.", jpgFile );
            
            showMessage( gameName ":  Error",
                         message,
                         true );
            delete [] message;
            }
        
        delete [] name;

        delete [] steamTxtName;
        delete [] jpgName;
        }
    
    // now we have a list of mods to upload (either new ones or changed ones)
    
    SteamAPICall_t hSteamAPICall = 
        SteamUGC()->CreateItem( SteamUtils()->GetAppID(), 
                                k_EWorkshopFileTypeCommunity );
    
    // FIXME:  register handler for call result
    // see sample class above
    // do we need to run some function repeatedly to step call results in steam?

    // yes, seems like we should run
    // SteamAPI_RunCallbacks()  in a loop that sleeps 10x per second
    // ah, yes, we do that above
    
    deallocateFileVector( &oxzFiles );
    deallocateFileVector( &steamTxtFiles );
    deallocateFileVector( &jpgFiles );
    }


void processModDownloads() {
    }




void processMods() {
    AppLog::info( "Processing mods before launching the game." );
    
    processModUploads();
    
    processModDownloads();
    }



