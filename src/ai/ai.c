/**
 * @file ai.c
 * @brief AI integration implementation using Google Gemini API
 * 
 * AIshA - Advanced Intelligent Shell Assistant
 * 
 * Uses OpenSSL for HTTPS requests and cJSON for JSON parsing.
 * API key loaded from GEMINI_API_KEY env var or ~/.aisharc file.
 * Uses structured JSON output for reliable shell command generation.
 */

#include "ai.h"
#include "cJSON.h"
#include "colors.h"
#include "shell.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/utsname.h>

/*============================================================================
 * Static Variables
 *============================================================================*/

static char* g_api_key = NULL;
static int g_ai_initialized = 0;

/* System prompts for different request types */
static const char* PROMPT_TRANSLATE = 
    "You are a shell command translator for AIshA (Advanced Intelligent Shell Assistant). "
    "Convert the user's natural language request into a valid shell command. "
    "Consider the user's current working directory and system information provided. "
    "Return ONLY a single shell command that can be executed directly. "
    "For file searches, use find or ls commands. "
    "For text searches, use grep. "
    "If you cannot translate the request into a command, set success to false and explain why.";

static const char* PROMPT_EXPLAIN = 
    "You are a shell command expert for AIshA. "
    "Explain what the given command does in simple, clear terms. "
    "Break down each part of the command (flags, arguments, pipes). "
    "Be concise but thorough. Use markdown formatting.";

static const char* PROMPT_FIX = 
    "You are a shell debugging assistant for AIshA. "
    "The user ran a command that produced an error. "
    "Analyze the error and provide a corrected command. "
    "Explain briefly what went wrong and why the fix works.";

static const char* PROMPT_CHAT = 
    "You are AIshA (Advanced Intelligent Shell Assistant), a helpful AI integrated "
    "into a Unix shell. Help users with shell commands, scripting, and system administration. "
    "Keep responses concise and practical. You can use markdown formatting.";

/*============================================================================
 * HTTP/SSL Implementation
 *============================================================================*/

typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} http_buffer_t;

static http_buffer_t* http_buffer_new(void) {
    http_buffer_t* buf = malloc(sizeof(http_buffer_t));
    if (!buf) return NULL;
    buf->capacity = 8192;
    buf->data = malloc(buf->capacity);
    buf->size = 0;
    if (buf->data) buf->data[0] = '\0';
    return buf;
}

static void http_buffer_append(http_buffer_t* buf, const char* data, size_t len) {
    if (buf->size + len + 1 > buf->capacity) {
        buf->capacity = (buf->size + len + 1) * 2;
        buf->data = realloc(buf->data, buf->capacity);
    }
    if (buf->data) {
        memcpy(buf->data + buf->size, data, len);
        buf->size += len;
        buf->data[buf->size] = '\0';
    }
}

static void http_buffer_free(http_buffer_t* buf) {
    if (buf) {
        free(buf->data);
        free(buf);
    }
}

/**
 * Make an HTTPS POST request using OpenSSL
 */
static char* https_post(const char* host, const char* path, const char* json_body) {
    SSL_CTX* ctx = NULL;
    SSL* ssl = NULL;
    int sockfd = -1;
    char* result = NULL;
    
    /* Initialize OpenSSL */
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        return NULL;
    }
    
    /* Resolve hostname */
    struct hostent* server = gethostbyname(host);
    if (!server) {
        SSL_CTX_free(ctx);
        return NULL;
    }
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        SSL_CTX_free(ctx);
        return NULL;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(443);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        SSL_CTX_free(ctx);
        return NULL;
    }
    
    /* Create SSL connection */
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sockfd);
    
    if (SSL_connect(ssl) <= 0) {
        SSL_free(ssl);
        close(sockfd);
        SSL_CTX_free(ctx);
        return NULL;
    }
    
    /* Build HTTP request */
    size_t content_len = strlen(json_body);
    size_t request_size = strlen(path) + strlen(host) + content_len + 256;
    char* request = malloc(request_size);
    if (!request) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(sockfd);
        SSL_CTX_free(ctx);
        return NULL;
    }
    
    snprintf(request, request_size,
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        path, host, content_len, json_body);
    
    /* Send request */
    SSL_write(ssl, request, strlen(request));
    free(request);
    
    /* Read response */
    http_buffer_t* response = http_buffer_new();
    char buf[4096];
    int bytes;
    
    while ((bytes = SSL_read(ssl, buf, sizeof(buf) - 1)) > 0) {
        buf[bytes] = '\0';
        http_buffer_append(response, buf, bytes);
    }
    
    /* Extract body (after \r\n\r\n) */
    if (response && response->data && response->size > 0) {
        char* body = strstr(response->data, "\r\n\r\n");
        if (body) {
            body += 4; /* Skip past header separator */
            
            /* Handle chunked transfer encoding */
            /* Check if the response uses chunked encoding by looking for chunk size */
            /* Chunk format: <hex size>\r\n<data>\r\n */
            char* json_start = body;
            
            /* Skip chunk size line if present (e.g., "229\r\n") */
            char* newline = strstr(body, "\r\n");
            if (newline && (newline - body) < 10) {
                /* Check if the first part looks like a hex number */
                int is_hex = 1;
                for (char* p = body; p < newline && is_hex; p++) {
                    if (!((*p >= '0' && *p <= '9') || 
                          (*p >= 'a' && *p <= 'f') || 
                          (*p >= 'A' && *p <= 'F'))) {
                        is_hex = 0;
                    }
                }
                if (is_hex && newline > body) {
                    json_start = newline + 2; /* Skip chunk size and \r\n */
                }
            }
            
            /* Find the actual JSON start (first '{' or '[') */
            while (*json_start && *json_start != '{' && *json_start != '[') {
                json_start++;
            }
            
            if (*json_start) {
                result = strdup(json_start);
                
                /* Remove trailing chunk markers if present */
                if (result) {
                    size_t len = strlen(result);
                    /* Remove trailing "\r\n0\r\n\r\n" (end of chunked encoding) */
                    while (len > 0 && (result[len-1] == '\r' || result[len-1] == '\n' || 
                                       result[len-1] == '0')) {
                        result[--len] = '\0';
                    }
                }
            }
        }
    }
    
    /* Cleanup */
    http_buffer_free(response);
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sockfd);
    SSL_CTX_free(ctx);
    
    return result;
}

/*============================================================================
 * API Key Management
 *============================================================================*/

static int load_api_key_from_env(void) {
    char* key = getenv("GEMINI_API_KEY");
    if (key && strlen(key) > 0) {
        if (g_api_key) free(g_api_key);
        g_api_key = strdup(key);
        return 0;
    }
    return -1;
}

static int load_api_key_from_config(void) {
    if (!g_home_directory) return -1;
    
    char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/.aisharc", g_home_directory);
    
    FILE* f = fopen(config_path, "r");
    if (!f) return -1;
    
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        
        if (strncmp(line, "GEMINI_API_KEY=", 15) == 0) {
            char* key = line + 15;
            size_t len = strlen(key);
            while (len > 0 && (key[len-1] == '\n' || key[len-1] == '\r' || 
                               key[len-1] == '"' || key[len-1] == '\'')) {
                key[--len] = '\0';
            }
            if (key[0] == '"' || key[0] == '\'') key++;
            
            if (strlen(key) > 0) {
                if (g_api_key) free(g_api_key);
                g_api_key = strdup(key);
                fclose(f);
                return 0;
            }
        }
    }
    
    fclose(f);
    return -1;
}

/*============================================================================
 * System Context
 *============================================================================*/

static char* get_system_context(void) {
    static char context[2048];
    
    char cwd[1024];
    if (!getcwd(cwd, sizeof(cwd))) {
        strcpy(cwd, "unknown");
    }
    
    struct utsname uts;
    const char* os_name = "Linux";
    const char* os_release = "";
    if (uname(&uts) == 0) {
        os_name = uts.sysname;
        os_release = uts.release;
    }
    
    snprintf(context, sizeof(context),
        "System Context:\n"
        "- Shell: AIshA (Advanced Intelligent Shell Assistant)\n"
        "- Current Directory: %s\n"
        "- Operating System: %s %s\n"
        "- User: %s\n"
        "- Home Directory: %s\n",
        cwd,
        os_name, os_release,
        g_username ? g_username : "user",
        g_home_directory ? g_home_directory : "~"
    );
    
    return context;
}

/* Note: Structured JSON schemas removed - using simpler responseMimeType approach */

/*============================================================================
 * Public Functions
 *============================================================================*/

int ai_init(void) {
    if (load_api_key_from_env() == 0 || load_api_key_from_config() == 0) {
        g_ai_initialized = 1;
        return 0;
    }
    return -1;
}

int ai_available(void) {
    return g_ai_initialized && g_api_key != NULL;
}

void ai_cleanup(void) {
    if (g_api_key) {
        free(g_api_key);
        g_api_key = NULL;
    }
    g_ai_initialized = 0;
}

const char* ai_get_masked_key(void) {
    static char masked[32];
    if (!g_api_key) return "(not set)";
    
    size_t len = strlen(g_api_key);
    if (len <= 8) {
        snprintf(masked, sizeof(masked), "****");
    } else {
        snprintf(masked, sizeof(masked), "%.4s...%.4s", 
                 g_api_key, g_api_key + len - 4);
    }
    return masked;
}

/*============================================================================
 * Core AI Request with Structured Output
 *============================================================================*/

/* Debug mode - set via environment AI_DEBUG=1 */
static int ai_debug_enabled(void) {
    char* debug = getenv("AI_DEBUG");
    return debug && strcmp(debug, "1") == 0;
}

static cJSON* ai_request_json(ai_request_type_t type, const char* input, int use_schema) {
    if (!ai_available()) {
        if (ai_debug_enabled()) fprintf(stderr, "[AI DEBUG] Not available\n");
        return NULL;
    }
    
    /* Select system prompt based on type */
    const char* system_prompt;
    switch (type) {
        case AI_REQUEST_TRANSLATE: system_prompt = PROMPT_TRANSLATE; break;
        case AI_REQUEST_EXPLAIN:   system_prompt = PROMPT_EXPLAIN; break;
        case AI_REQUEST_FIX:       system_prompt = PROMPT_FIX; break;
        case AI_REQUEST_CHAT:
        case AI_REQUEST_SUGGEST:
        default:                   system_prompt = PROMPT_CHAT; break;
    }
    
    /* Build full prompt with system context */
    char* context = get_system_context();
    char full_prompt[AI_MAX_PROMPT_SIZE];
    snprintf(full_prompt, sizeof(full_prompt), "%s\n\nUser request: %s", context, input);
    
    /* Build JSON request body */
    cJSON* root = cJSON_CreateObject();
    
    /* System instruction */
    cJSON* system_instruction = cJSON_CreateObject();
    cJSON* system_parts = cJSON_CreateArray();
    cJSON* system_text = cJSON_CreateObject();
    cJSON_AddStringToObject(system_text, "text", system_prompt);
    cJSON_AddItemToArray(system_parts, system_text);
    cJSON_AddItemToObject(system_instruction, "parts", system_parts);
    cJSON_AddItemToObject(root, "system_instruction", system_instruction);
    
    /* User content */
    cJSON* contents = cJSON_CreateArray();
    cJSON* content = cJSON_CreateObject();
    cJSON* parts = cJSON_CreateArray();
    cJSON* text_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(text_obj, "text", full_prompt);
    cJSON_AddItemToArray(parts, text_obj);
    cJSON_AddItemToObject(content, "parts", parts);
    cJSON_AddStringToObject(content, "role", "user");
    cJSON_AddItemToArray(contents, content);
    cJSON_AddItemToObject(root, "contents", contents);
    
    /* Generation config - simpler approach without schema enforcement */
    if (use_schema) {
        cJSON* gen_config = cJSON_CreateObject();
        cJSON_AddStringToObject(gen_config, "responseMimeType", "application/json");
        cJSON_AddItemToObject(root, "generationConfig", gen_config);
    }
    
    char* json_body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!json_body) {
        if (ai_debug_enabled()) fprintf(stderr, "[AI DEBUG] Failed to create JSON body\n");
        return NULL;
    }
    
    if (ai_debug_enabled()) {
        fprintf(stderr, "[AI DEBUG] Request body length: %zu\n", strlen(json_body));
    }
    
    /* Build API URL with key */
    char path[512];
    snprintf(path, sizeof(path), 
             "/v1beta/models/gemini-2.5-flash:generateContent?key=%s", 
             g_api_key);
    
    /* Make request */
    char* http_response = https_post("generativelanguage.googleapis.com", path, json_body);
    free(json_body);
    
    if (!http_response) {
        if (ai_debug_enabled()) fprintf(stderr, "[AI DEBUG] HTTPS request failed\n");
        return NULL;
    }
    
    if (ai_debug_enabled()) {
        fprintf(stderr, "[AI DEBUG] Response length: %zu\n", strlen(http_response));
        fprintf(stderr, "[AI DEBUG] Response preview: %.500s...\n", http_response);
    }
    
    /* Parse response */
    cJSON* resp_json = cJSON_Parse(http_response);
    free(http_response);
    
    if (!resp_json) {
        if (ai_debug_enabled()) fprintf(stderr, "[AI DEBUG] Failed to parse JSON response\n");
        return NULL;
    }
    
    /* Check for error */
    cJSON* error = cJSON_GetObjectItem(resp_json, "error");
    if (error) {
        if (ai_debug_enabled()) {
            char* error_str = cJSON_Print(error);
            fprintf(stderr, "[AI DEBUG] API Error: %s\n", error_str);
            free(error_str);
        }
        cJSON_Delete(resp_json);
        return NULL;
    }
    
    /* Extract response text */
    cJSON* candidates = cJSON_GetObjectItem(resp_json, "candidates");
    if (candidates && cJSON_IsArray(candidates) && cJSON_GetArraySize(candidates) > 0) {
        cJSON* candidate = cJSON_GetArrayItem(candidates, 0);
        cJSON* content_resp = cJSON_GetObjectItem(candidate, "content");
        if (content_resp) {
            cJSON* parts_resp = cJSON_GetObjectItem(content_resp, "parts");
            if (parts_resp && cJSON_IsArray(parts_resp) && cJSON_GetArraySize(parts_resp) > 0) {
                cJSON* part = cJSON_GetArrayItem(parts_resp, 0);
                cJSON* text = cJSON_GetObjectItem(part, "text");
                if (text && cJSON_IsString(text)) {
                    const char* text_value = text->valuestring;
                    
                    if (ai_debug_enabled()) {
                        fprintf(stderr, "[AI DEBUG] Got text response: %.200s\n", text_value);
                    }
                    
                    /* Try to parse as JSON */
                    cJSON* result = cJSON_Parse(text_value);
                    
                    if (result) {
                        /* Check if it parsed to a string (e.g., "\"ls -a\"" -> "ls -a") */
                        if (cJSON_IsString(result)) {
                            /* It's a JSON string - wrap it in an object */
                            /* IMPORTANT: strdup before cJSON_Delete to avoid use-after-free */
                            char* cmd_copy = strdup(result->valuestring);
                            cJSON_Delete(result);
                            result = cJSON_CreateObject();
                            cJSON_AddBoolToObject(result, "success", 1);
                            cJSON_AddStringToObject(result, "command", cmd_copy);
                            if (ai_debug_enabled()) {
                                fprintf(stderr, "[AI DEBUG] Wrapped string result: %s\n", cmd_copy);
                            }
                            free(cmd_copy);
                        }
                        /* If it's already an object, use it as-is */
                        cJSON_Delete(resp_json);
                        return result;
                    }
                    
                    /* If not valid JSON at all, use raw text */
                    result = cJSON_CreateObject();
                    cJSON_AddBoolToObject(result, "success", 1);
                    cJSON_AddStringToObject(result, "command", text_value);
                    if (ai_debug_enabled()) {
                        fprintf(stderr, "[AI DEBUG] Using raw text as command\n");
                    }
                    cJSON_Delete(resp_json);
                    return result;
                }
            }
        }
    }
    
    if (ai_debug_enabled()) fprintf(stderr, "[AI DEBUG] Could not extract text from response\n");
    cJSON_Delete(resp_json);
    return NULL;
}

ai_response_t* ai_request(ai_request_type_t type, const char* input) {
    ai_response_t* response = malloc(sizeof(ai_response_t));
    if (!response) return NULL;
    
    response->text = NULL;
    response->error = NULL;
    response->success = 0;
    
    if (!ai_available()) {
        response->error = strdup("AI not available. Set GEMINI_API_KEY.");
        return response;
    }
    
    /* For chat, don't use structured output */
    cJSON* result = ai_request_json(type, input, 0);
    
    if (!result) {
        response->error = strdup("Failed to get AI response");
        return response;
    }
    
    /* Extract text */
    cJSON* cmd = cJSON_GetObjectItem(result, "command");
    if (cmd && cJSON_IsString(cmd)) {
        response->text = strdup(cmd->valuestring);
        response->success = 1;
    }
    
    cJSON_Delete(result);
    return response;
}

char* ai_translate(const char* natural_language) {
    cJSON* result = ai_request_json(AI_REQUEST_TRANSLATE, natural_language, 1);
    
    if (!result) {
        return NULL;
    }
    
    cJSON* success = cJSON_GetObjectItem(result, "success");
    cJSON* command = cJSON_GetObjectItem(result, "command");
    
    char* ret = NULL;
    
    /* Check for structured response */
    if (command && cJSON_IsString(command)) {
        /* Clean the command - remove code blocks if present */
        char* cmd = command->valuestring;
        while (*cmd && (*cmd == ' ' || *cmd == '\n' || *cmd == '`')) cmd++;
        char* cmd_copy = strdup(cmd);
        if (cmd_copy) {
            /* Remove trailing backticks and whitespace */
            size_t len = strlen(cmd_copy);
            while (len > 0 && (cmd_copy[len-1] == '`' || cmd_copy[len-1] == '\n' || cmd_copy[len-1] == ' ')) {
                cmd_copy[--len] = '\0';
            }
            /* Remove "bash" or "sh" prefix if present */
            if (strncmp(cmd_copy, "bash", 4) == 0 || strncmp(cmd_copy, "sh", 2) == 0) {
                char* newline = strchr(cmd_copy, '\n');
                if (newline) {
                    ret = strdup(newline + 1);
                    free(cmd_copy);
                } else {
                    ret = cmd_copy;
                }
            } else {
                ret = cmd_copy;
            }
        }
    } else if (success && cJSON_IsFalse(success)) {
        cJSON* explanation = cJSON_GetObjectItem(result, "explanation");
        if (explanation && cJSON_IsString(explanation)) {
            char error_msg[1024];
            snprintf(error_msg, sizeof(error_msg), "ERROR: %s", explanation->valuestring);
            ret = strdup(error_msg);
        }
    }
    
    cJSON_Delete(result);
    return ret;
}

char* ai_explain(const char* command) {
    char prompt[AI_MAX_PROMPT_SIZE];
    snprintf(prompt, sizeof(prompt), "Explain this command: %s", command);
    
    cJSON* result = ai_request_json(AI_REQUEST_EXPLAIN, prompt, 1);
    
    if (!result) {
        return NULL;
    }
    
    /* Build explanation from response */
    char* explanation = malloc(4096);
    if (!explanation) {
        cJSON_Delete(result);
        return NULL;
    }
    explanation[0] = '\0';
    
    /* Check for command field (used as text container) */
    cJSON* cmd = cJSON_GetObjectItem(result, "command");
    if (cmd && cJSON_IsString(cmd)) {
        strncpy(explanation, cmd->valuestring, 4095);
        explanation[4095] = '\0';
    }
    
    /* Also check for structured fields */
    cJSON* summary = cJSON_GetObjectItem(result, "summary");
    if (summary && cJSON_IsString(summary)) {
        snprintf(explanation, 4096, "**Summary:** %s\n\n", summary->valuestring);
    }
    
    cJSON* breakdown = cJSON_GetObjectItem(result, "breakdown");
    if (breakdown && cJSON_IsArray(breakdown)) {
        strncat(explanation, "**Breakdown:**\n", 4096 - strlen(explanation) - 1);
        int count = cJSON_GetArraySize(breakdown);
        for (int i = 0; i < count; i++) {
            cJSON* item = cJSON_GetArrayItem(breakdown, i);
            if (item && cJSON_IsString(item)) {
                char line[512];
                snprintf(line, sizeof(line), "  • %s\n", item->valuestring);
                strncat(explanation, line, 4096 - strlen(explanation) - 1);
            }
        }
    }
    
    cJSON_Delete(result);
    return explanation;
}

char* ai_fix(const char* error_message, const char* command) {
    char prompt[AI_MAX_PROMPT_SIZE];
    snprintf(prompt, sizeof(prompt), 
             "Command that failed: %s\nError message: %s\nPlease diagnose and fix.", 
             command, error_message);
    
    cJSON* result = ai_request_json(AI_REQUEST_FIX, prompt, 1);
    
    if (!result) {
        return NULL;
    }
    
    /* Build fix response */
    char* fix = malloc(4096);
    if (!fix) {
        cJSON_Delete(result);
        return NULL;
    }
    fix[0] = '\0';
    
    /* Check for command field (used as text container) */
    cJSON* cmd = cJSON_GetObjectItem(result, "command");
    if (cmd && cJSON_IsString(cmd)) {
        strncpy(fix, cmd->valuestring, 4095);
        fix[4095] = '\0';
        cJSON_Delete(result);
        return fix;
    }
    
    cJSON* diagnosis = cJSON_GetObjectItem(result, "diagnosis");
    if (diagnosis && cJSON_IsString(diagnosis)) {
        snprintf(fix, 4096, "**Problem:** %s\n\n", diagnosis->valuestring);
    }
    
    cJSON* fixed_cmd = cJSON_GetObjectItem(result, "fixed_command");
    if (fixed_cmd && cJSON_IsString(fixed_cmd)) {
        char line[512];
        snprintf(line, sizeof(line), "**Fixed command:**\n  %s\n\n", fixed_cmd->valuestring);
        strncat(fix, line, 4096 - strlen(fix) - 1);
    }
    
    cJSON* explanation = cJSON_GetObjectItem(result, "explanation");
    if (explanation && cJSON_IsString(explanation)) {
        char line[1024];
        snprintf(line, sizeof(line), "**Why:** %s", explanation->valuestring);
        strncat(fix, line, 4096 - strlen(fix) - 1);
    }
    
    cJSON_Delete(result);
    return fix;
}

char* ai_chat(const char* message) {
    ai_response_t* resp = ai_request(AI_REQUEST_CHAT, message);
    char* result = NULL;
    
    if (resp) {
        if (resp->success && resp->text) {
            result = strdup(resp->text);
        } else if (resp->error) {
            result = strdup(resp->error);
        }
        ai_response_free(resp);
    }
    
    return result;
}

void ai_response_free(ai_response_t* response) {
    if (response) {
        free(response->text);
        free(response->error);
        free(response);
    }
}
