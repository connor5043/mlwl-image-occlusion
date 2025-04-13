#define _POSIX_C_SOURCE 200809L
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <limits.h>

// Structure for holding the list of courses
typedef struct {
    char **course_names;
    int *course_ids;
    int count;
} CourseList;

#define AUTH_TOKEN_MAX_LEN 256
#define MAX_COURSES 100

// Structure to store a rectangle
typedef struct {
    SDL_Rect rect;
    SDL_Color color; // Color of the rectangle
} DrawableRect;

#define MAX_RECTS 100

// Helper function to decode Unicode escape sequences in a string
void decode_unicode(char *dest, const char *src) {
    while (*src) {
        if (*src == '\\' && *(src + 1) == 'u') {
            unsigned int codepoint;
            if (sscanf(src + 2, "%4x", &codepoint) == 1) {
                if (codepoint <= 0x7F) {
                    *dest++ = codepoint;
                } else if (codepoint <= 0x7FF) {
                    *dest++ = 0xC0 | (codepoint >> 6);
                    *dest++ = 0x80 | (codepoint & 0x3F);
                } else if (codepoint <= 0xFFFF) {
                    *dest++ = 0xE0 | (codepoint >> 12);
                    *dest++ = 0x80 | ((codepoint >> 6) & 0x3F);
                    *dest++ = 0x80 | (codepoint & 0x3F);
                }
                src += 6;
                continue;
            }
        }
        *dest++ = *src++;
    }
    *dest = '\0';
}

// Function to hide password input
void get_hidden_password(char *password, size_t max_len) {
    struct termios oldt, newt;

    // Save old terminal settings
    if (tcgetattr(STDIN_FILENO, &oldt) != 0) {
        perror("tcgetattr");
        exit(EXIT_FAILURE);
    }

    newt = oldt;
    newt.c_lflag &= ~(ECHO); // Disable echo

    if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) != 0) {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }

    printf("Enter your MLWL password: ");
    fflush(stdout); // Ensure the prompt is displayed immediately

    // Read password
    if (fgets(password, max_len, stdin) == NULL) {
        perror("fgets");
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // Restore terminal settings
        exit(EXIT_FAILURE);
    }

    // Remove trailing newline character, if present
    size_t len = strlen(password);
    if (len > 0 && password[len - 1] == '\n') {
        password[len - 1] = '\0';
    }

    // Restore old terminal settings
    if (tcsetattr(STDIN_FILENO, TCSANOW, &oldt) != 0) {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }

    printf("\n"); // Add a newline after password entry
}

// Callback function to handle libcurl responses
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    strncat((char *)userp, (char *)contents, realsize);
    return realsize;
}

// Function to log in to MLWL and retrieve the auth token
bool login_to_mlwl(const char *username, const char *password, char *auth_token) {
    CURL *curl;
    CURLcode res;
    bool success = false;

    curl = curl_easy_init();
    if (curl) {
        // JSON payload
        char post_fields[256];
        snprintf(post_fields, sizeof(post_fields), "{\"username\":\"%s\",\"password\":\"%s\"}", username, password);

        // Response buffer
        char response[1024] = {0};

        // Set the URL and POST data
        curl_easy_setopt(curl, CURLOPT_URL, "https://backend.mylittlewordland.com/api/log-in");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);

        // Set up callback to write response data
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

        // Set headers
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Execute the request
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            // Parse the response for "authToken"
            char *token_start = strstr(response, "\"authToken\":\"");
            if (token_start) {
                token_start += strlen("\"authToken\":\""); // Move past the "authToken":" part
                char *token_end = strchr(token_start, '\"');
                if (token_end) {
                    size_t token_len = token_end - token_start;
                    if (token_len < AUTH_TOKEN_MAX_LEN) {
                        strncpy(auth_token, token_start, token_len);
                        auth_token[token_len] = '\0';
                        success = true;
                    }
                }
            }
        }

        // Clean up
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    return success;
}

// Function to get the list of courses
bool get_courses(const char *auth_token, CourseList *course_list) {
    CURL *curl;
    CURLcode res;
    bool success = false;

    curl = curl_easy_init();
    if (curl) {
        // Response buffer
        char response[4096] = {0};

        // Set the URL
        curl_easy_setopt(curl, CURLOPT_URL, "https://backend.mylittlewordland.com/api/learner/dashboard");

        // Set up callback to write response data
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

        // Set headers, including the auth token
        struct curl_slist *headers = NULL;
        char auth_header[AUTH_TOKEN_MAX_LEN + 16];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", auth_token);
        headers = curl_slist_append(headers, auth_header);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Execute the request
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            // Parse the JSON response to extract course IDs and names
            const char *start = response;
            course_list->count = 0;

            while ((start = strstr(start, "{\"id\":")) != NULL && course_list->count < MAX_COURSES) {
                int id;
                char name[256];
                char raw_name[256];

                if (sscanf(start, "{\"id\":%d,\"name\":\"%[^\"]", &id, raw_name) == 2) {
                    course_list->course_ids[course_list->count] = id;
                    decode_unicode(name, raw_name);

                    course_list->course_names[course_list->count] = strdup(name);
                    course_list->count++;
                }
                start++;
            }
            success = true;
        }

        // Clean up
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    return success;
}

// Function to fetch the column ID from the course details
int get_column_id(const char *auth_token, int course_id) {
    CURL *curl;
    CURLcode res;
    int column_id = -1;

    curl = curl_easy_init();
    if (curl) {
        // URL for course details
        char url[256];
        snprintf(url, sizeof(url), "https://backend.mylittlewordland.com/api/course/%d/entries", course_id);

        // Response buffer
        char response[4096] = {0};

        // Set the URL
        curl_easy_setopt(curl, CURLOPT_URL, url);

        // Set up callback to write response data
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

        // Set headers, including the auth token
        struct curl_slist *headers = NULL;
        char auth_header[AUTH_TOKEN_MAX_LEN + 16];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", auth_token);
        headers = curl_slist_append(headers, auth_header);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Execute the request
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            // Parse the JSON response to extract the column id
            const char *start = strstr(response, "\"columns\":[");
            if (start) {
                start = strstr(start, "{\"id\":");
                if (start) {
                    start = strstr(start + 1, "{\"id\":");
                    if (start) {
                        sscanf(start, "{\"id\":%d", &column_id);
                    }
                }
            }
        }

        // Clean up
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    return column_id;
}

// Function to free the course list
void free_courses(CourseList *course_list) {
    for (int i = 0; i < course_list->count; i++) {
        free(course_list->course_names[i]);
    }
    free(course_list->course_ids);
    free(course_list->course_names);
}

// Function to save the modified image
void save_image(SDL_Surface *surface, const char *file_name) {
    if (IMG_SavePNG(surface, file_name) != 0) {
        printf("Unable to save image %s! SDL_image Error: %s\n", file_name, IMG_GetError());
    } else {
        printf("Image saved successfully: %s\n", file_name);
    }
}

// Function to crop a surface to the specified rectangle
SDL_Surface *crop_surface(SDL_Surface *surface, SDL_Rect rect) {
    SDL_Surface *cropped = SDL_CreateRGBSurfaceWithFormat(0, rect.w, rect.h, 32, surface->format->format);
    if (!cropped) {
        printf("Unable to create cropped surface! SDL_Error: %s\n", SDL_GetError());
        return NULL;
    }

    SDL_Rect dest_rect = {0, 0, rect.w, rect.h};
    if (SDL_BlitSurface(surface, &rect, cropped, &dest_rect) != 0) {
        printf("Unable to crop surface! SDL_Error: %s\n", SDL_GetError());
        SDL_FreeSurface(cropped);
        return NULL;
    }

    return cropped;
}

// Function to find the script path
void get_script_path(char *script_path, size_t size) {
    const char *local_script = "./script.sh";
    const char *default_script_dir = ".local/share/mlwl-image-occlusion/script.sh";

    // Check if the script exists in the current directory
    if (access(local_script, F_OK) == 0) {
        realpath(local_script, script_path); // Get the absolute path of the local script
    } else {
        // Fall back to ~/.local/share/example-name/script.sh
        const char *home_dir = getenv("HOME"); // Get the user's home directory
        if (!home_dir) {
            fprintf(stderr, "Error: Unable to determine the home directory.\n");
            exit(1);
        }
        snprintf(script_path, size, "%s/%s", home_dir, default_script_dir);
        if (access(script_path, F_OK) != 0) {
            fprintf(stderr, "Error: Script not found in either location.\n");
            exit(1);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <image_path>\n", argv[0]);
        return 1;
    }
    
    char script_path[PATH_MAX];

    // Get the script path (either local or in ~/.local/share/example-name)
    get_script_path(script_path, sizeof(script_path));
    // printf("Using script: %s\n", script_path);

    // Prompt user for MLWL credentials
    char username[128];
    char password[128];
    char auth_token[AUTH_TOKEN_MAX_LEN] = {0};
    CourseList course_list = {0};
    course_list.course_names = malloc(MAX_COURSES * sizeof(char *));
    course_list.course_ids = malloc(MAX_COURSES * sizeof(int));

    printf("Enter your MLWL username: ");
    scanf("%127s", username);
    while (getchar() != '\n'); // Flush the input buffer
    get_hidden_password(password, sizeof(password));

    if (!login_to_mlwl(username, password, auth_token)) {
        fprintf(stderr, "Error: Login failed. Please check your credentials.\n");
        free_courses(&course_list);
        return 1;
    }

    if (!get_courses(auth_token, &course_list)) {
        fprintf(stderr, "Error: Failed to retrieve courses.\n");
        free_courses(&course_list);
        return 1;
    }

    // Display the list of courses
    printf("\nCourses:\n");
    for (int i = 0; i < course_list.count; i++) {
        printf("%d. %s\n", i + 1, course_list.course_names[i]);
    }

    // Prompt the user to select a course
    int course_choice = 0;
    while (course_choice < 1 || course_choice > course_list.count) {
        printf("\nSelect a course by number: ");
        scanf("%d", &course_choice);
    }
    int selected_course_id = course_list.course_ids[course_choice - 1];
    printf("You selected: %s (ID: %d)\n", course_list.course_names[course_choice - 1], selected_course_id);

    // Fetch the column ID for the selected course
    int column_id = get_column_id(auth_token, selected_course_id);
    if (column_id == -1) {
        fprintf(stderr, "Warning: Failed to retrieve column ID. Continuing with the program.\n");
    } else {
        printf("Column ID of the second column: %d\n", column_id);
    }

    // Free allocated memory for courses
    free_courses(&course_list);

    // Load the image for rectangle drawing
    const char *image_path = argv[1];
    SDL_Surface *image_surface = IMG_Load(image_path);
    if (!image_surface) {
        printf("Unable to load image %s! SDL_image Error: %s\n", image_path, IMG_GetError());
        return 1;
    }

    // Initialize SDL window and renderer
    SDL_Window *window = SDL_CreateWindow("MLWL Image Occlusion", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, image_surface->w, image_surface->h, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture *image_texture = SDL_CreateTextureFromSurface(renderer, image_surface);

    DrawableRect rectangles[MAX_RECTS];
    int rect_count = 0;

    bool drawing = false;
    SDL_Point start_point = {0, 0};
    SDL_Rect current_rect = {0, 0, 0, 0};
    bool quit = false;
    SDL_Event e;

    // Main interaction loop
    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button == SDL_BUTTON_LEFT && rect_count < MAX_RECTS) {
                    drawing = true;
                    start_point.x = e.button.x;
                    start_point.y = e.button.y;
                }
            } else if (e.type == SDL_MOUSEBUTTONUP) {
                if (e.button.button == SDL_BUTTON_LEFT && drawing) {
                    drawing = false;
                    current_rect.w = abs(e.button.x - start_point.x);
                    current_rect.h = abs(e.button.y - start_point.y);
                    current_rect.x = (e.button.x < start_point.x) ? e.button.x : start_point.x;
                    current_rect.y = (e.button.y < start_point.y) ? e.button.y : start_point.y;

                    rectangles[rect_count].rect = current_rect;
                    rectangles[rect_count].color = (SDL_Color){128, 128, 128, 255}; // Grey color
                    rect_count++;
                }
            } else if (e.type == SDL_MOUSEMOTION) {
                if (drawing) {
                    current_rect.w = abs(e.motion.x - start_point.x);
                    current_rect.h = abs(e.motion.y - start_point.y);
                    current_rect.x = (e.motion.x < start_point.x) ? e.motion.x : start_point.x;
                    current_rect.y = (e.motion.y < start_point.y) ? e.motion.y : start_point.y;
                }
            }
        }

        // Render the image and rectangles
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, image_texture, NULL, NULL);

        for (int i = 0; i < rect_count; ++i) {
            SDL_SetRenderDrawColor(renderer, rectangles[i].color.r, rectangles[i].color.g, rectangles[i].color.b, rectangles[i].color.a);
            SDL_RenderFillRect(renderer, &rectangles[i].rect);
        }

        if (drawing) {
            SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
            SDL_RenderDrawRect(renderer, &current_rect);
        }
        SDL_RenderPresent(renderer);
    }

    // Save the images
    for (int i = 0; i < rect_count; ++i) {
        // Save grey-filled image
        SDL_Surface *grey_surface = SDL_ConvertSurface(image_surface, image_surface->format, 0);
        if (!grey_surface) {
            printf("Unable to create grey surface! SDL_Error: %s\n", SDL_GetError());
            continue;
        }
        SDL_FillRect(grey_surface, &rectangles[i].rect, SDL_MapRGB(grey_surface->format, 128, 128, 128));

        char grey_filename[256];
        snprintf(grey_filename, sizeof(grey_filename), "output_rectangle_%d.png", i + 1);
        save_image(grey_surface, grey_filename);
        SDL_FreeSurface(grey_surface);

        // Save cropped image
        SDL_Surface *cropped_surface = crop_surface(image_surface, rectangles[i].rect);
        if (!cropped_surface) continue;

        char cropped_filename[256];
        snprintf(cropped_filename, sizeof(cropped_filename), "cropped_rectangle_%d.png", i + 1);
        save_image(cropped_surface, cropped_filename);
        SDL_FreeSurface(cropped_surface);
    }

    SDL_FreeSurface(image_surface);
    SDL_DestroyTexture(image_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    bool debug_mode = false;

    // Check if --debug flag is passed
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            debug_mode = true;
            break;
        }
    }
    
    char course_id_str[16]; // Buffer to hold the COURSE_ID string
    char column_id_str[16]; // Buffer to hold the COLUMN_ID string
    sprintf(course_id_str, "%d", selected_course_id);
    sprintf(column_id_str, "%d", column_id);
    setenv("COURSE_ID", course_id_str, 1);
    setenv("COLUMN_ID", column_id_str, 1);
    setenv("AUTH_TOKEN", auth_token, 1);

    if (debug_mode) {
        // In debug mode, execute the script in the current process and show output
        printf("Running script in debug mode...\n");
        if (execlp("bash", "bash", script_path, (char *)NULL) == -1) {
            perror("Error running script in debug mode");
            return 1;
        }
    } else {
        // In non-debug mode, fork a new process to run the script
        pid_t pid = fork();

        if (pid < 0) {
            // Fork failed
            perror("Error forking process");
            return 1;
        } else if (pid == 0) {
            // Child process
            // Detach from the terminal
            if (setsid() == -1) {
                perror("Error creating new session");
                exit(1);
            }

            // Redirect stdout and stderr to /dev/null
            freopen("/dev/null", "w", stdout);
            freopen("/dev/null", "w", stderr);

            // Run the script
            if (execlp("bash", "bash", script_path, (char *)NULL) == -1) {
                perror("Error running script in detached mode");
                exit(1); // Exit child process if exec fails
            }
        } else {
            // Parent process
            printf("Script is running in detached mode (PID: %d).\n", pid);
            // Do not wait for the child process; immediately return control to the terminal
        }
    }

    // Rest of the program
    return 0;
}

