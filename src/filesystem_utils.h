#ifndef FILESYSTEM_UTILS_H
#define FILESYSTEM_UTILS_H

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdio.h>
#include <string>
#include <vector>

#if _WIN32
#include "win32dirent.h"
#include <windows.h>
#else // _WIN32
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif // _WIN32

#if __APPLE__
#include <mach-o/dyld.h>
#endif

#if _WIN32
typedef std::wstring path_t;
#define PATHSTR(X) L##X
#else
typedef std::string path_t;
#define PATHSTR(X) X
#endif

static bool compare_path_natural(const path_t &a, const path_t &b) {
  if (a.empty() || b.empty()) {
    return a.empty() && !b.empty();
  }

  char charA = a[0];
  char charB = b[0];

  if (std::isdigit(charA) && !std::isdigit(charB)) {
    return true;
  } else if (!std::isdigit(charA) && std::isdigit(charB)) {
    return false;
  } else if (!std::isdigit(charA) && !std::isdigit(charB)) {
    if (std::toupper(charA) == std::toupper(charB)) {
      return compare_path_natural(a.substr(1), b.substr(1));
    }
    return (std::toupper(charA) < std::toupper(charB));
  }

  std::istringstream issa(a);
  std::istringstream issb(b);
  int ia, ib;
  issa >> ia;
  issb >> ib;
  if (ia != ib) {
    return ia < ib;
  }

  std::string anew, bnew;
  std::getline(issa, anew);
  std::getline(issb, bnew);

  return compare_path_natural(anew, bnew);
}

#if _WIN32
static bool path_is_directory(const path_t &path) {
  DWORD attr = GetFileAttributesW(path.c_str());
  return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

static int list_directory(const path_t &dirpath,
                          std::vector<path_t> &imagepaths) {
  imagepaths.clear();

  _WDIR *dir = _wopendir(dirpath.c_str());
  if (!dir) {
    fwprintf(stderr, L"opendir failed %ls\n", dirpath.c_str());
    return -1;
  }

  struct _wdirent *ent = 0;
  while ((ent = _wreaddir(dir))) {
    if (ent->d_type != DT_REG)
      continue;

    imagepaths.push_back(path_t(ent->d_name));
  }

  _wclosedir(dir);
  std::sort(imagepaths.begin(), imagepaths.end(), compare_path_natural);

  return 0;
}
#else  // _WIN32
static bool path_is_directory(const path_t &path) {
  struct stat s;
  if (stat(path.c_str(), &s) != 0)
    return false;
  return S_ISDIR(s.st_mode);
}

static int list_directory(const path_t &dirpath,
                          std::vector<path_t> &imagepaths) {
  imagepaths.clear();

  DIR *dir = opendir(dirpath.c_str());
  if (!dir) {
    fprintf(stderr, "opendir failed %s\n", dirpath.c_str());
    return -1;
  }

  struct dirent *ent = 0;
  while ((ent = readdir(dir))) {
    if (ent->d_type != DT_REG)
      continue;

    imagepaths.push_back(path_t(ent->d_name));
  }

  closedir(dir);
  std::sort(imagepaths.begin(), imagepaths.end(), compare_path_natural);

  return 0;
}
#endif // _WIN32

static path_t get_file_name_without_extension(const path_t &path) {
  size_t dot = path.rfind(PATHSTR('.'));
  if (dot == path_t::npos)
    return path;

  return path.substr(0, dot);
}

static path_t get_file_extension(const path_t &path) {
  size_t dot = path.rfind(PATHSTR('.'));
  if (dot == path_t::npos)
    return path_t();

  return path.substr(dot + 1);
}

#if _WIN32
static path_t get_executable_directory() {
  wchar_t filepath[256];
  GetModuleFileNameW(NULL, filepath, 256);

  wchar_t *backslash = wcsrchr(filepath, L'\\');
  backslash[1] = L'\0';

  return path_t(filepath);
}
#elif __APPLE__
static path_t get_executable_directory() {
  char filepath[256];
  uint32_t size = sizeof(filepath);
  _NSGetExecutablePath(filepath, &size);

  char *slash = strrchr(filepath, '/');
  slash[1] = '\0';

  return path_t(filepath);
}
#else
static path_t get_executable_directory() {
  char filepath[256];
  readlink("/proc/self/exe", filepath, 256);

  char *slash = strrchr(filepath, '/');
  slash[1] = '\0';

  return path_t(filepath);
}
#endif

static bool filepath_is_readable(const path_t &path) {
#if _WIN32
  FILE *fp = _wfopen(path.c_str(), L"rb");
#else  // _WIN32
  FILE *fp = fopen(path.c_str(), "rb");
#endif // _WIN32
  if (!fp)
    return false;

  fclose(fp);
  return true;
}

static path_t sanitize_filepath(const path_t &path) {
  if (filepath_is_readable(path))
    return path;

  return get_executable_directory() + path;
}

static path_t sanitize_dirpath(const path_t &path) {
  if (path_is_directory(path))
    return path;

  return get_executable_directory() + path;
}

#endif // FILESYSTEM_UTILS_H
