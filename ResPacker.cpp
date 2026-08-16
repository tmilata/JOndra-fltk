/*
 * ResPacker.cpp - multiplatformni resource packer pro JOndru
 *
 * C++98 / Visual C++ 6.0 / GCC compatible.
 *
 * Packer NIKDY nerozbaluje obrazky. Vsechny resources uklada do vystupu
 * presne v puvodni binarni podobe:
 *
 *   <asset-root>/images  - PNG soubory zustanou PNG
 *   <asset-root>/roms    - binarni data beze zmeny
 *   <asset-root>/sound   - binarni data beze zmeny
 *
 * Vystup:
 *   EmbeddedResourcesData.cpp
 *
 * Pouziti:
 *   ResPacker <asset-root> <output-cpp>
 *
 * Obe cesty mohou byt relativni nebo absolutni. Relativni cesty se
 * vyhodnocuji vuci aktualnimu pracovnimu adresari procesu.
 *
 * Priklady:
 *   ResPacker.exe ..\Release ..\EmbeddedResourcesData.cpp
 *   ./ResPacker ./resources ./build/EmbeddedResourcesData.cpp
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#ifdef _WIN32
# include <windows.h>
#else
# include <dirent.h>
# include <sys/stat.h>
# include <sys/types.h>
#endif

enum ResourceKind {
    RESOURCE_BINARY = 0,
    RESOURCE_PNG = 1
};

struct SourceFile {
    std::string diskPath;
    std::string resourceName;
    ResourceKind kind;
};

struct PackedResource {
    std::string name;
    unsigned long offset;
    unsigned long size;
    ResourceKind kind;
};

static std::string normalizeSlashes(const std::string& value) {
    std::string out(value);
    std::string::size_type i;
    for (i = 0; i < out.size(); ++i) {
        if (out[i] == '\\') {
            out[i] = '/';
        }
    }
    return out;
}

static std::string joinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;

    char last = a[a.size() - 1];
    if (last == '/' || last == '\\') return a + b;

#ifdef _WIN32
    return a + "\\" + b;
#else
    return a + "/" + b;
#endif
}

static bool isDirectory(const std::string& path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES &&
           (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

static bool hasResourceDirectories(const std::string& root) {
    return isDirectory(joinPath(root, "images")) &&
           isDirectory(joinPath(root, "roms")) &&
           isDirectory(joinPath(root, "sound"));
}

static bool endsWithIgnoreCase(const std::string& text, const char* suffix) {
    size_t suffixLen = strlen(suffix);
    if (text.size() < suffixLen) return false;

    size_t start = text.size() - suffixLen;
    size_t i;
    for (i = 0; i < suffixLen; ++i) {
        unsigned char a = (unsigned char)text[start + i];
        unsigned char b = (unsigned char)suffix[i];
        if (a >= 'A' && a <= 'Z') a = (unsigned char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (unsigned char)(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

static bool sourceFileLess(const SourceFile& a, const SourceFile& b) {
    return a.resourceName < b.resourceName;
}

static void addSourceFile(const std::string& diskPath,
                          const std::string& resourceName,
                          ResourceKind kind,
                          std::vector<SourceFile>& files) {
    SourceFile f;
    f.diskPath = diskPath;
    f.resourceName = normalizeSlashes(resourceName);
    f.kind = kind;
    files.push_back(f);
}

#ifdef _WIN32

static bool scanDirectoryRecursive(const std::string& diskDir,
                                   const std::string& resourceDir,
                                   ResourceKind kind,
                                   std::vector<SourceFile>& files) {
    std::string pattern = joinPath(diskDir, "*");
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "CHYBA: Nelze otevrit adresar: %s\n", diskDir.c_str());
        return false;
    }

    bool ok = true;
    do {
        const char* name = fd.cFileName;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

        std::string diskPath = joinPath(diskDir, name);
        std::string resPath = resourceDir + "/" + name;

        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if (!scanDirectoryRecursive(diskPath, resPath, kind, files)) {
                ok = false;
                break;
            }
        } else {
            /* V images zabalujeme pouze PNG. ondra.ico se tedy automaticky preskoci. */
            if (kind == RESOURCE_PNG && !endsWithIgnoreCase(name, ".png")) {
                printf("  preskakuji %-36s (images: podporuji jen PNG)\n",
                       normalizeSlashes(resPath).c_str());
                continue;
            }
            addSourceFile(diskPath, resPath, kind, files);
        }
    } while (FindNextFileA(h, &fd) != 0);

    FindClose(h);
    return ok;
}

#else

static bool scanDirectoryRecursive(const std::string& diskDir,
                                   const std::string& resourceDir,
                                   ResourceKind kind,
                                   std::vector<SourceFile>& files) {
    DIR* dir = opendir(diskDir.c_str());
    if (dir == 0) {
        fprintf(stderr, "CHYBA: Nelze otevrit adresar: %s\n", diskDir.c_str());
        return false;
    }

    bool ok = true;
    struct dirent* ent;
    while ((ent = readdir(dir)) != 0) {
        const char* name = ent->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

        std::string diskPath = joinPath(diskDir, name);
        std::string resPath = resourceDir + "/" + name;

        struct stat st;
        if (stat(diskPath.c_str(), &st) != 0) {
            fprintf(stderr, "CHYBA: Nelze zjistit typ souboru: %s\n", diskPath.c_str());
            ok = false;
            break;
        }

        if (S_ISDIR(st.st_mode)) {
            if (!scanDirectoryRecursive(diskPath, resPath, kind, files)) {
                ok = false;
                break;
            }
        } else if (S_ISREG(st.st_mode)) {
            if (kind == RESOURCE_PNG && !endsWithIgnoreCase(name, ".png")) {
                printf("  preskakuji %-36s (images: podporuji jen PNG)\n",
                       normalizeSlashes(resPath).c_str());
                continue;
            }
            addSourceFile(diskPath, resPath, kind, files);
        }
    }

    closedir(dir);
    return ok;
}

#endif

static bool readBinaryFile(const std::string& path,
                           std::vector<unsigned char>& data) {
    FILE* f = fopen(path.c_str(), "rb");
    if (f == 0) {
        fprintf(stderr, "CHYBA: Nelze otevrit soubor: %s\n", path.c_str());
        return false;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        fprintf(stderr, "CHYBA: Nelze zjistit velikost souboru: %s\n", path.c_str());
        return false;
    }

    long length = ftell(f);
    if (length < 0) {
        fclose(f);
        fprintf(stderr, "CHYBA: Nelze zjistit velikost souboru: %s\n", path.c_str());
        return false;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        fprintf(stderr, "CHYBA: Nelze cist soubor: %s\n", path.c_str());
        return false;
    }

    data.resize((size_t)length);
    if (length > 0) {
        size_t got = fread(&data[0], 1, (size_t)length, f);
        if (got != (size_t)length) {
            fclose(f);
            fprintf(stderr, "CHYBA: Neuplne nacteni souboru: %s\n", path.c_str());
            return false;
        }
    }

    fclose(f);
    return true;
}

static bool packFiles(const std::vector<SourceFile>& files,
                      std::vector<unsigned char>& blob,
                      std::vector<PackedResource>& packed) {
    size_t i;
    for (i = 0; i < files.size(); ++i) {
        const SourceFile& src = files[i];
        std::vector<unsigned char> data;

        if (!readBinaryFile(src.diskPath, data)) return false;

        if (blob.size() > 0xFFFFFFFFUL ||
            data.size() > 0xFFFFFFFFUL ||
            blob.size() + data.size() > 0xFFFFFFFFUL) {
            fprintf(stderr, "CHYBA: Resource blob prekrocil 4 GB.\n");
            return false;
        }

        PackedResource item;
        item.name = src.resourceName;
        item.offset = (unsigned long)blob.size();
        item.size = (unsigned long)data.size();
        item.kind = src.kind;
        packed.push_back(item);

        if (!data.empty()) blob.insert(blob.end(), data.begin(), data.end());

        printf("  %-40s %8lu B  %s\n",
               src.resourceName.c_str(),
               item.size,
               src.kind == RESOURCE_PNG ? "PNG" : "BIN");
    }
    return true;
}

static void writeEscapedCString(FILE* f, const std::string& text) {
    fputc('"', f);
    size_t i;
    for (i = 0; i < text.size(); ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c == '\\' || c == '"') {
            fputc('\\', f);
            fputc(c, f);
        } else if (c >= 32 && c <= 126) {
            fputc(c, f);
        } else {
            /* Pevne 3-ciferne octal escape nema problem s nasledujicim znakem. */
            fprintf(f, "\\%03o", (unsigned int)c);
        }
    }
    fputc('"', f);
}

static bool writeGeneratedCpp(const std::string& path,
                              const std::vector<unsigned char>& blob,
                              const std::vector<PackedResource>& packed) {
    FILE* f = fopen(path.c_str(), "wb");
    if (f == 0) {
        fprintf(stderr, "CHYBA: Nelze vytvorit vystup: %s\n", path.c_str());
        return false;
    }

    fprintf(f,
        "/*\r\n"
        " * GENERATED FILE - DO NOT EDIT.\r\n"
        " * Vytvoreno programem ResPacker z images/, roms/ a sound/.\r\n"
        " * Data jsou ulozena v puvodni binarni podobe (PNG zustava PNG).\r\n"
        " */\r\n\r\n"
        "#include \"EmbeddedResources.h\"\r\n\r\n");

    /*
     * Zamerne pouzivame ciselny initializer misto jednoho obrovskeho stringu.
     * Stary MSVC 6.0 ma omezeni velikosti string literal. Binarka bude mit
     * pouze velikost puvodnich souboru; velikost tohoto generovaneho .cpp
     * je pouze build-time zalezitost.
     */
    fprintf(f, "const unsigned char gEmbeddedResourceData[] = {\r\n");
    size_t i;
    for (i = 0; i < blob.size(); ++i) {
        if ((i % 16U) == 0U) fprintf(f, "    ");
        fprintf(f, "0x%02X", (unsigned int)blob[i]);
        if (i + 1U != blob.size()) fprintf(f, ",");
        if ((i % 16U) == 15U || i + 1U == blob.size()) {
            fprintf(f, "\r\n");
        } else {
            fprintf(f, " ");
        }
    }
    fprintf(f, "};\r\n\r\n");

    fprintf(f, "const unsigned long gEmbeddedResourceDataSize = %luUL;\r\n\r\n",
            (unsigned long)blob.size());

    fprintf(f, "const EmbeddedResourceEntry gEmbeddedResources[] = {\r\n");
    for (i = 0; i < packed.size(); ++i) {
        const PackedResource& item = packed[i];
        fprintf(f, "    { ");
        writeEscapedCString(f, item.name);
        fprintf(f, ", %luUL, %luUL, %s }",
                item.offset,
                item.size,
                item.kind == RESOURCE_PNG
                    ? "EMBEDDED_RESOURCE_PNG"
                    : "EMBEDDED_RESOURCE_BINARY");
        if (i + 1U != packed.size()) fprintf(f, ",");
        fprintf(f, "\r\n");
    }
    fprintf(f, "};\r\n\r\n");

    fprintf(f, "const unsigned int gEmbeddedResourceCount = %uU;\r\n",
            (unsigned int)packed.size());

    if (fclose(f) != 0) {
        fprintf(stderr, "CHYBA: Chyba pri uzavirani vystupu: %s\n", path.c_str());
        return false;
    }
    return true;
}

static bool filesEqual(const std::string& a, const std::string& b) {
    FILE* fa = fopen(a.c_str(), "rb");
    if (fa == 0) return false;

    FILE* fb = fopen(b.c_str(), "rb");
    if (fb == 0) {
        fclose(fa);
        return false;
    }

    bool equal = true;
    unsigned char ba[16384];
    unsigned char bb[16384];

    for (;;) {
        size_t na = fread(ba, 1, sizeof(ba), fa);
        size_t nb = fread(bb, 1, sizeof(bb), fb);

        if (na != nb) {
            equal = false;
            break;
        }
        if (na == 0) break;
        if (memcmp(ba, bb, na) != 0) {
            equal = false;
            break;
        }
    }

    fclose(fa);
    fclose(fb);
    return equal;
}

static bool replaceIfChanged(const std::string& tempPath,
                             const std::string& finalPath,
                             bool& changed) {
    if (filesEqual(tempPath, finalPath)) {
        remove(tempPath.c_str());
        changed = false;
        return true;
    }

    /* Windows rename() neprepise existujici soubor. */
    remove(finalPath.c_str());
    if (rename(tempPath.c_str(), finalPath.c_str()) != 0) {
        fprintf(stderr, "CHYBA: Nelze prejmenovat %s na %s\n",
                tempPath.c_str(), finalPath.c_str());
        return false;
    }

    changed = true;
    return true;
}

static void printUsage(const char* exe) {
    printf("Pouziti:\n");
    printf("  %s <asset-root> <output-cpp>\n\n", exe);
    printf("Asset root musi obsahovat images, roms a sound.\n");
    printf("Obe cesty mohou byt relativni nebo absolutni.\n");
    printf("Relativni cesty se vyhodnocuji vuci aktualnimu working directory.\n\n");
    printf("Priklad Windows:\n");
    printf("  %s ..\\Release ..\\EmbeddedResourcesData.cpp\n\n", exe);
    printf("Priklad Linux:\n");
    printf("  %s ./resources ./build/EmbeddedResourcesData.cpp\n", exe);
}

static int runPacker(int argc, char* argv[])
{
	if (argc == 2 &&
        (strcmp(argv[1], "-h") == 0 ||
		strcmp(argv[1], "--help") == 0 ||
		strcmp(argv[1], "/?") == 0)) {
        printUsage(argv[0]);
        return 0;
    }
	
    if (argc != 3) {
        printUsage(argv[0]);
        return 2;
    }
	
    std::string assetRoot = argv[1];
    std::string outputPath = argv[2];
	
    if (!hasResourceDirectories(assetRoot)) {
        fprintf(stderr,
			"CHYBA: Asset root neobsahuje vsechny adresare images, roms a sound: %s\n",
			assetRoot.c_str());
        return 1;
    }
	
    std::string tempPath = outputPath + ".tmp";
	
    printf("JOndra ResPacker\n");
    printf("Asset root : %s\n", assetRoot.c_str());
    printf("Output     : %s\n\n", outputPath.c_str());
	
    std::vector<SourceFile> files;
	
    printf("Hledam resources...\n");
    if (!scanDirectoryRecursive(joinPath(assetRoot, "images"), "images",
		RESOURCE_PNG, files)) return 1;
    if (!scanDirectoryRecursive(joinPath(assetRoot, "roms"), "roms",
		RESOURCE_BINARY, files)) return 1;
    if (!scanDirectoryRecursive(joinPath(assetRoot, "sound"), "sound",
		RESOURCE_BINARY, files)) return 1;
	
    std::sort(files.begin(), files.end(), sourceFileLess);
	
    if (files.empty()) {
        fprintf(stderr, "CHYBA: Nebyly nalezeny zadne resources.\n");
        return 1;
    }
	
    printf("\nBalim %u resources v puvodni binarni podobe...\n",
		(unsigned int)files.size());
	
    std::vector<unsigned char> blob;
    std::vector<PackedResource> packed;
    if (!packFiles(files, blob, packed)) return 1;
	
    printf("\nCelkem: %u resources, %lu B embedded dat\n",
		(unsigned int)packed.size(),
		(unsigned long)blob.size());
	
    remove(tempPath.c_str());
    if (!writeGeneratedCpp(tempPath, blob, packed)) {
        remove(tempPath.c_str());
        return 1;
    }
	
    bool changed = false;
    if (!replaceIfChanged(tempPath, outputPath, changed)) {
        remove(tempPath.c_str());
        return 1;
    }
	
    if (changed) {
        printf("Vygenerovano: %s\n", outputPath.c_str());
    } else {
        printf("Beze zmen: %s\n", outputPath.c_str());
    }
	
    return 0;

}

#ifdef _WIN32

#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return runPacker(__argc, __argv);
}

#else

int main(int argc, char* argv[])
{
    return runPacker(argc, argv);
}

#endif

