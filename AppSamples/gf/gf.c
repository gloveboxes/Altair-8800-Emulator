#include <stdio.h>

#define GF_VERSION "1.3"
#define GMSREPO "https://raw.githubusercontent.com/AzureSphereCloudEnabledAltair8800/RetroGames/main"


/* Function prototypes */
int save_ep_url();
int validate_endpoint();
int str_tolower();

/* --- Begin dxweb.c inlined --- */
#define WG_IDX_RESET 109
#define WWG_SET_URL  112
#define WG_FILENAME  114
#define WG_EP_NAME   110

#define WG_STATUS    33
#define WG_EOF       0
#define WG_FAILED    3
#define WG_DATAREADY 2
#define WG_GET_BYTE  201
#define WG_WAITING   1

int inp();
int outp();
int fputc();

int dxwebfn(filename, len, endpoint)
char *filename;
int len;
int endpoint;
{
    int c;

    outp(WWG_SET_URL, endpoint);
    outp(WG_IDX_RESET, 0);

    for (c = 0; c < len; c++)
    {
        outp(WG_FILENAME, filename[c]);
    }
    outp(WG_FILENAME, 0);
    return 0;
}

int dxwebcpy(fp_output, bytes_written)
FILE *fp_output;
int *bytes_written;
{
    int status;
    int count;
    int next_byte;

    count = 0;

    while (1)
    {
        status = inp(WG_STATUS) & 255;
        if (status == WG_EOF)
        {
            break;
        }

        if (status == WG_FAILED)
        {
            return -1;
        }

        if (status == WG_DATAREADY)
        {
            next_byte = inp(WG_GET_BYTE) & 255;
            fputc(next_byte, fp_output);
            count++;
        }
        else if (status == WG_WAITING)
        {
            /* nothing to do */
        }
    }

    if (bytes_written != 0)
    {
        *bytes_written = count;
    }

    return 0;
}

int dxseturl(endpoint, len)
char *endpoint;
int len;
{
    int c;

    outp(WG_IDX_RESET, 0);
    for (c = 0; c < len; c++)
    {
        outp(WG_EP_NAME, endpoint[c]);
    }
    outp(WG_EP_NAME, 0);
    return 0;
}
/* --- End dxweb.c inlined --- */

FILE *fp_output;
char *endpoint;
char *filename;
char file_content[128];

int defaults()
{
    FILE *fp;
    int len;

    /* Load endpoint from gf.txt */
    fp = fopen("gf.txt", "r");
    if (fp != NULL)
    {
        if (fgets(file_content, 128, fp) != NULL)
        {
            len = strlen(file_content);
            if (len > 1 && len < 128)
            {
                /* Remove newline */
                if (file_content[len - 1] == '\n')
                {
                    file_content[len - 1] = 0x00;
                    len--;
                }

                dxseturl(file_content, len);
            }
        }
        fclose(fp);
        printf("Default endpoint loaded from gf.txt: %s\n", file_content);
    }
    return 0;
}

int main(argc, argv)
int argc;
char **argv;
{
    int wg_result, bytes_written;

    wg_result = 0;
    bytes_written = 0;

    defaults();

    if (argc == 1)
    {
        printf("GF (Get File) - File Transfer Utility v%s\n", GF_VERSION);
        printf("Transfer files from web over HTTP(s)\n\n");
        printf("Usage: gf [--help] [--version] [-e <url>] [-f <filename>] [-g <gamefile>]\n");
        printf("\nOptions:\n");
        printf("  --help       Show this help message\n");
        printf("  --version    Show version information\n");
        printf("  -e <url>     Set a custom HTTP/HTTPS endpoint URL\n");
        printf("               The URL will be used for web-based file transfers\n");
        printf("  -f <filename> Download a specific file from the configured endpoint\n");
        printf("  -g <gamefile> Download a game file from the built-in games repository\n");
        printf("\nExamples:\n");
        printf("  gf -e http://localhost:5500     Set local development server\n");
        printf("  gf -e https://example.com/files Set remote HTTPS endpoint\n");
        printf("  gf -f myfile.txt                Download myfile.txt from configured endpoint\n");
        printf("  gf -g love.bas                  Download love.bas from games repository\n");
        return 0;
    }

    if (argc == 3)
    {
        char *save_filename;
        char *path_iter;
        
        if (strcmp(argv[1], "-e") == 0 || strcmp(argv[1], "-E") == 0)
        {
            endpoint = argv[2];
            if (validate_endpoint(endpoint) == 0)
            {
                save_ep_url(endpoint);
                printf("\nEndpoint URL set to: %s\n", endpoint);
                return 0;
            }
            else
            {
                printf("Invalid endpoint URL. Must start with 'http'.\n");
                return -1;
            }
        }
        else if (strcmp(argv[1], "-f") == 0 || strcmp(argv[1], "-F") == 0)
        {

            filename = argv[2];
            save_filename = filename;
            path_iter = filename;

            while (*path_iter != '\0')
            {
                if (*path_iter == '/' || *path_iter == '\\')
                {
                    if (*(path_iter + 1) != '\0')
                    {
                        save_filename = path_iter + 1;
                    }
                }
                path_iter++;
            }

            if (strlen(file_content) == 0)
            {
                printf("Error: No endpoint URL found in gf.txt. Use -e to set an endpoint first.\n");
                return -1;
            }



            printf("\nDownloading file '%s' from URL '%s'\n", filename, file_content);
            if (save_filename != filename)
            {
                printf("Saving as '%s'\n", save_filename);
            }

            if ((fp_output = fopen(save_filename, "w")) == NULL)
            {
                printf("Error: Failed to create output file '%s'\n", save_filename);
                printf("Check disk space and write permissions.\n");
                return -1;
            }
            
            dxwebfn(filename, strlen(filename), 0);
            wg_result = dxwebcpy(fp_output, &bytes_written);
            if (wg_result == 0)
            {
                printf(" done (%d bytes)\n", bytes_written);
            }
            else
            {
                printf(" failed\n");
            }

            fclose(fp_output);
            if (wg_result == -1)
            {
                printf("\n\nWeb copy failed for file '%s'. Check filename and network connection\n", save_filename);
                unlink(save_filename);
            }
            return 0;
        }
        else if (strcmp(argv[1], "-g") == 0 || strcmp(argv[1], "-G") == 0)
        {
            filename = argv[2];
            save_filename = filename;
            path_iter = filename;

            while (*path_iter != '\0')
            {
                if (*path_iter == '/' || *path_iter == '\\')
                {
                    if (*(path_iter + 1) != '\0')
                    {
                        save_filename = path_iter + 1;
                    }
                }
                path_iter++;
            }

            printf("\nDownloading game '%s' from games repository\n", filename);
            if (save_filename != filename)
            {
                printf("Saving as '%s'\n", save_filename);
            }

            if ((fp_output = fopen(save_filename, "w")) == NULL)
            {
                printf("Error: Failed to create output file '%s'\n", save_filename);
                printf("Check disk space and write permissions.\n");
                return -1;
            }
            
            /* Set games repository as the custom endpoint */
            dxseturl(GMSREPO, strlen(GMSREPO));
            dxwebfn(filename, strlen(filename), 0);
            wg_result = dxwebcpy(fp_output, &bytes_written);
            if (wg_result == 0)
            {
                printf(" done (%d bytes)\n", bytes_written);
            }
            else
            {
                printf(" failed\n");
            }

            fclose(fp_output);
            if (wg_result == -1)
            {
                printf("\n\nGame download failed for file '%s'. Check filename and network connection\n", save_filename);
                unlink(save_filename);
            }
            return 0;
        }
        else
        {
            printf("Unrecognized option: %s\n", argv[1]);
            printf("Usage: gf [--help] [-e <url>] [-f <filename>] [-g <gamefile>]\n");
            return -1;
        }
    }

    if (argc == 2)
    {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "--HELP") == 0)
        {
            printf("GF (Get File) - File Transfer Utility v%s\n", GF_VERSION);
            printf("Transfer files from web over HTTP(s)\n\n");
            printf("Usage: gf [--help] [--version] [-e <url>] [-f <filename>] [-g <gamefile>]\n");
            printf("\nOptions:\n");
            printf("  --help       Show this help message\n");
            printf("  --version    Show version information\n");
            printf("  -e <url>     Set a custom HTTP/HTTPS endpoint URL\n");
            printf("               The URL will be used for web-based file transfers\n");
            printf("  -f <filename> Download a specific file from the configured endpoint\n");
            printf("  -g <gamefile> Download a game file from the built-in games repository\n");
            printf("\nExamples:\n");
            printf("  gf -e http://localhost:5500     Set local development server\n");
            printf("  gf -e https://example.com/files Set remote HTTPS endpoint\n");
            printf("  gf -f myfile.txt                Download myfile.txt from configured endpoint\n");
            printf("  gf -g love.bas                  Download love.bas from games repository\n");
            return 0;
        }

        if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "--VERSION") == 0)
        {
            printf("GF (Get File) version %s\n", GF_VERSION);
            printf("Compiled for BDS C 1.6 on CP/M\n");
            return 0;
        }

        /* Invalid option */
        printf("Invalid option: %s\n", argv[1]);
        printf("Use 'gf --help' for usage information.\n");
        return -1;
    }

    return 0;
}



int validate_endpoint(url)
char *url;
{
    char check_url[6];
    int c;
    int len;

    if (url == NULL)
        return -1;

    len = 0;
    while (url[len] != '\0')
        len++;

    if (len < 8 || len > 120)
        return -1;

    /* Check for http:// or https:// (CP/M always upper case) */
    for (c = 0; c < 4 && c < len; c++)
        check_url[c] = url[c];
    check_url[4] = '\0';

    if (check_url[0] == 'H' && check_url[1] == 'T' && check_url[2] == 'T' && check_url[3] == 'P')
    {
        /* Verify it has :// after http */
        if (len > 6 && url[4] == ':' && url[5] == '/' && url[6] == '/')
            return 0;
    }
    return -1;
}

int save_ep_url(endpoint)
char *endpoint;
{
    FILE *fp;
    /* write endpoint to gf.txt */
    fp = fopen("gf.txt", "w");
    if (fp != NULL)
    {
        int i;
        i = 0;
        while (endpoint[i] != '\0')
        {
            fputc(endpoint[i], fp);
            i++;
        }
        fputc('\n', fp);
        fclose(fp);
    }
    return 0;
}


