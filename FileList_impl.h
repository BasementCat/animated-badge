#ifndef FILELIST_IMPL_H
#define FILELIST_IMPL_H

#include <SD.h>
#include "prefs.h"

#define GIF_FILE 1
#define BMP_FILE 2
#define ANIM_FILE 4
#define QOIF2_FILE 8

class FileList {
    public:
        bool is_gif = false, is_bmp = false, is_anim = false, is_qoif2 = false;

        FileList(const char* directory) {
            this->directory = directory;
            // this->read_num_files(0, true);
        }

        void init(Prefs* prefs) {
            this->read_num_files(0, true, prefs->last_filename);
        }

        void init() {
            this->read_num_files(0, true);
        }

        int get_num_files() {
            return this->num_files;
        }

        int get_index() {
            return this->index;
        }

        const char* get_cur_file() {
            return this->filename;
        }

        void set_file(int index) {
            this->read_num_files(index, true);
        }

        void next_file(Prefs* prefs) {
            this->change_file(prefs, 1, true);
        }

        void prev_file(Prefs* prefs) {
            this->change_file(prefs, -1, true);
        }

    private:
        const char* directory;
        char filename[128];
        int num_files = 0, index = 0;

        void change_file(Prefs* prefs, int dir, bool set_index) {
            this->read_num_files(this->index + dir, set_index);
            if (prefs != NULL) {
                set_pref_last_filename(prefs, (const char *)this->filename);
                write_prefs(prefs);
            }
        }

        void read_num_files(int index, bool set_index, char* last_filename) {
            int count = 0, curindex = -1;
            uint16_t ftype;

            if (set_index) {
                if (index >= this->num_files) {
                    index = 0;
                } else if (index < 0) {
                    index = this->num_files - 1;
                }
            }

            File directory = SD.open(this->directory);
            if (!directory) {
                return;
            }

            File file = directory.openNextFile();
            while (file) {
                ftype = this->is_anim_file(file.name());
                if (ftype) {
                    count++;
                    curindex++;
                    if (set_index && ((last_filename != NULL && strcmp((char*)file.name(), last_filename) == 0 || (last_filename == NULL && index == curindex)))) {
                        this->is_gif = ftype & GIF_FILE;
                        this->is_bmp = ftype & BMP_FILE;
                        this->is_anim = ftype & ANIM_FILE;
                        this->is_qoif2 = ftype & QOIF2_FILE;
                        this->index = curindex;
#if !defined(ESP32)
                        // Copy the directory name into the pathname buffer - ESP32 SD Library includes the full path name in the filename, so no need to add the directory name
                        strcpy(this->filename, this->directory);
                        // Append the filename to the pathname
                        strcat(this->filename, (char*)file.name());
#else
                        strcpy(this->filename, (char*)file.name());
#endif
                    }
                }
                file.close();
                file = directory.openNextFile();
            }

            file.close();
            directory.close();

            this->num_files = count;
        }

        void read_num_files(int index, bool set_index) {
            this->read_num_files(index, set_index, NULL);
        }

        uint16_t is_anim_file(const char* filename) {
            String filename_string(filename);
            uint16_t out = 0;

#if defined(ESP32)
            // ESP32 filename includes the full path, so need to remove the path before looking at the filename
            int pathindex = filename_string.lastIndexOf("/");
            if (pathindex >= 0)
                filename_string.remove(0, pathindex + 1);
#endif

            Serial.print("\"");
            Serial.print(filename_string);
            Serial.print("\"");

            if ((filename_string[0] == '_') || (filename_string[0] == '~') || (filename_string[0] == '.')) {
                Serial.println(" ignoring: leading _/~/. character");
                return 0;
            }

            filename_string.toUpperCase();
            if (filename_string.endsWith(String(".SDA")) == true)
                out = ANIM_FILE;
            else if (filename_string.endsWith(String(".QOX")) == true)
                out = QOIF2_FILE;
            // else if (filename_string.endsWith(String(".GIF")) == true)
            //     out = GIF_FILE;
            // else if (filename_string.endsWith(String(".BMP")) == true)
            //     out = BMP_FILE;
            else
                Serial.println(" ignoring: doesn't end with .GIF or .BMP or .SDA");

            Serial.println();

            return out;
        }
};

// File file;

// int numberOfFiles;


// int initSdCard(int chipSelectPin) {
//     // initialize the SD card at full speed
//     pinMode(chipSelectPin, OUTPUT);
//     if (!SD.begin(chipSelectPin))
//         return -1;
//     return 0;
// }

// bool isAnimationFile(const char filename []) {
//     String filenameString(filename);

// #if defined(ESP32)
//     // ESP32 filename includes the full path, so need to remove the path before looking at the filename
//     int pathindex = filenameString.lastIndexOf("/");
//     if(pathindex >= 0)
//         filenameString.remove(0, pathindex + 1);
// #endif

//     Serial.print(filenameString);

//     if ((filenameString[0] == '_') || (filenameString[0] == '~') || (filenameString[0] == '.')) {
//         Serial.println(" ignoring: leading _/~/. character");
//         return false;
//     }

//     filenameString.toUpperCase();
//     if (filenameString.endsWith(".GIF") != 1) {
//         Serial.println(" ignoring: doesn't end of .GIF");
//         return false;
//     }

//     Serial.println();

//     return true;
// }

// // Enumerate and possibly display the animated GIF filenames in GIFS directory
// int enumerateGIFFiles(const char *directoryName, boolean displayFilenames) {

//     numberOfFiles = 0;

//     File directory = SD.open(directoryName);
//     if (!directory) {
//         return -1;
//     }

//     File file = directory.openNextFile();
//     while (file) {
//         if (isAnimationFile(file.name())) {
//             numberOfFiles++;
//             if (displayFilenames) {
//                 Serial.println(file.name());
//             }
//         }
//         file.close();
//         file = directory.openNextFile();
//     }

//     file.close();
//     directory.close();

//     return numberOfFiles;
// }

// // Get the full path/filename of the GIF file with specified index
// void getGIFFilenameByIndex(const char *directoryName, int index, char *pnBuffer) {

//     char* filename;

//     // Make sure index is in range
//     if ((index < 0) || (index >= numberOfFiles))
//         return;

//     File directory = SD.open(directoryName);
//     if (!directory)
//         return;

//     File file = directory.openNextFile();
//     while (file && (index >= 0)) {
//         filename = (char*)file.name();

//         if (isAnimationFile(file.name())) {
//             index--;

// #if !defined(ESP32)
//             // Copy the directory name into the pathname buffer - ESP32 SD Library includes the full path name in the filename, so no need to add the directory name
//             strcpy(pnBuffer, directoryName);
//             // Append the filename to the pathname
//             strcat(pnBuffer, filename);
// #else
//             strcpy(pnBuffer, filename);
// #endif
//         }

//         file.close();
//         file = directory.openNextFile();
//     }

//     file.close();
//     directory.close();
// }

// int openGifFilenameByIndex(const char *directoryName, int index) {
//     char pathname[30];

//     getGIFFilenameByIndex(directoryName, index, pathname);
    
//     Serial.print("Pathname: ");
//     Serial.println(pathname);

//     if(file)
//         file.close();

//     // Attempt to open the file for reading
//     file = SD.open(pathname);
//     if (!file) {
//         Serial.println("Error opening GIF file");
//         return -1;
//     }

//     return 0;
// }


// // Return a random animated gif path/filename from the specified directory
// void chooseRandomGIFFilename(const char *directoryName, char *pnBuffer) {

//     int index = random(numberOfFiles);
//     getGIFFilenameByIndex(directoryName, index, pnBuffer);
// }


#endif