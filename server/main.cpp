#include <iostream>
#include <string>
#include <fstream>
#include <algorithm>
#include <filesystem> // Para crear la carpeta lrcs/
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;
namespace fs = std::filesystem;

// --- CONFIGURACIÓN DE CONSOLA ---
void fix_windows_console() {
#ifdef _WIN32
    system("chcp 65001 > nul");
#endif
}

// --- CALLBACK DE CURL ---
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// --- NÚCLEO: PETICIÓN A LA API ---
string buscar_en_api(const string& query) {
    CURL* curl = curl_easy_init();
    if (!curl) return "[]";

    string readBuffer;
    char* encoded_query = curl_easy_escape(curl, query.c_str(), query.length());
    string url = "https://lrclib.net/api/search?q=" + string(encoded_query);
    curl_free(encoded_query);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: ProyectoLyricsUTEC/1.0");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return "[]";
    return readBuffer;
}

// --- SISTEMA DE ARCHIVOS: GUARDAR LRC ---
void guardar_archivo_lrc(const string& titulo, const string& contenido) {
    string carpeta = "lrcs";

    // Si la carpeta no existe, la creamos
    if (!fs::exists(carpeta)) {
        fs::create_directory(carpeta);
    }

    // Limpiamos el nombre para que Windows no de error
    string nombre_limpio = titulo;
    replace_if(nombre_limpio.begin(), nombre_limpio.end(), [](char c) {
        return (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '\"' || c == '<' || c == '>' || c == '|');
    }, '_');

    // Armamos la ruta: lrcs/Nombre_Cancion.lrc
    string ruta_completa = carpeta + "/" + nombre_limpio + ".lrc";

    ofstream archivo(ruta_completa);
    if (archivo.is_open()) {
        archivo << contenido;
        archivo.close();
        cout << "💾 ¡ÉXITO! Archivo guardado en: " << ruta_completa << "\n";
    } else {
        cout << "❌ Error al intentar crear el archivo físico.\n";
    }
}

// --- OPCIÓN 1: MOSTRAR TOP 10 ---
void buscar_top_10() {
    string query;
    cout << "\n> Ingresa cancion o artista: ";
    getline(cin, query);

    cout << "\nBuscando en la base de datos...\n";
    string json_response = buscar_en_api(query);

    try {
        auto data = json::parse(json_response);
        if (data.empty()) {
            cout << "No se encontraron resultados.\n";
            return;
        }

        cout << "\n--- TOP RESULTADOS ---\n";
        int contador = 0;
        for (auto& item : data) {
            if (contador >= 10) break;

            string titulo = item["name"];
            string artista = item["artistName"];
            bool tiene_letra = !item["syncedLyrics"].is_null();

            cout << "[" << contador + 1 << "] " << titulo << " - " << artista
                 << (tiene_letra ? " (✔ LRC Disponible)" : " (❌ Sin LRC)") << "\n";
            contador++;
        }
        cout << "----------------------\n";
    } catch (json::parse_error& e) {
        cerr << "Error al leer los datos del servidor.\n";
    }
}

// --- OPCIÓN 2: DESCARGAR Y FABRICAR LRC (CON PLAN B) ---
void descargar_letra() {
    string query;
    cout << "\n> Ingresa la cancion EXACTA para descargar: ";
    getline(cin, query);

    cout << "\nBuscando en la base de datos...\n";
    string json_response = buscar_en_api(query);

    try {
        auto data = json::parse(json_response);

        if (data.empty()) {
            cout << "❌ No existe ningún registro de esa canción.\n";
            return;
        }

        string letra_final = "";
        bool es_sincronizada = false;
        json cancion_elegida;

        // PRIORIDAD 1: Buscar letra sincronizada (La mejor experiencia)
        for (auto& item : data) {
            if (!item["syncedLyrics"].is_null()) {
                cancion_elegida = item;
                letra_final = item["syncedLyrics"];
                es_sincronizada = true;
                break;
            }
        }

        // PRIORIDAD 2: Plan B (Solo texto, pero sirve para la carátula)
        if (!es_sincronizada) {
            for (auto& item : data) {
                if (!item["plainLyrics"].is_null()) {
                    cancion_elegida = item;
                    letra_final = item["plainLyrics"];
                    break;
                }
            }
        }

        // Si rescatamos alguna de las dos letras, procedemos
        if (!letra_final.empty()) {
            string titulo = cancion_elegida["name"];
            string artista = cancion_elegida["artistName"];

            string album = "";
            if (!cancion_elegida["albumName"].is_null()) {
                album = cancion_elegida["albumName"];
            }

            // 1. Fabricamos la cabecera mágica (Esto es lo que jala la imagen)
            string contenido_lrc = "";
            contenido_lrc += "[ti:" + titulo + "]\n";
            contenido_lrc += "[ar:" + artista + "]\n";
            if (!album.empty()) {
                contenido_lrc += "[al:" + album + "]\n";
            }

            // 2. Pegamos la letra (sea sincronizada o el texto normal)
            contenido_lrc += letra_final;

            // 3. Avisamos al usuario qué tipo de letra consiguió
            if (es_sincronizada) {
                cout << "\n✅ ¡Letra SINCRONIZADA encontrada para: " << titulo << "!\n";
            } else {
                cout << "\n⚠️ ATENCIÓN: Solo se encontró texto plano para: " << titulo << ".\n";
                cout << "Se descargó igual para que tengas la carátula y puedas leerla.\n";
            }

            // 4. Guardamos el archivo físico
            guardar_archivo_lrc(titulo, contenido_lrc);

        } else {
            cout << "❌ La canción existe en la base de datos, pero está marcada como INSTRUMENTAL o no tiene letra.\n";
        }

    } catch (json::parse_error& e) {
        cerr << "Error al procesar la búsqueda.\n";
    }
}

// --- MAIN LOOP ---
int main() {
    fix_windows_console();
    curl_global_init(CURL_GLOBAL_ALL);

    int opcion = 0;

    do {
        cout << "\n===================================";
        cout << "\n      🎶 LYRICSFY CONSOLE 🎶      ";
        cout << "\n===================================";
        cout << "\n1. Buscar una cancion (Ver top 10)";
        cout << "\n2. Descargar letra de una cancion";
        cout << "\n3. Salir";
        cout << "\n===================================";
        cout << "\nElige una opcion: ";

        cin >> opcion;
        cin.ignore();

        switch (opcion) {
            case 1: buscar_top_10(); break;
            case 2: descargar_letra(); break;
            case 3: cout << "Saliendo del sistema...\n"; break;
            default: cout << "Opcion invalida.\n";
        }
    } while (opcion != 3);

    curl_global_cleanup();
    return 0;
}