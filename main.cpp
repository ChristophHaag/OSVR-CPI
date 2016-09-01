/*
 * Copyright 2016 OSVR and contributors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "mainwindow.h"
#include <QApplication>
#include <iostream>

int main(int argc, char *argv[])
{
    bool verbose = false;
    for(int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            std::cout << "Enabling verbose mode" << std::endl;
            verbose = true;
        }
    }
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling); // DPI support

    QApplication a(argc, argv);
    MainWindow w(verbose);
    w.show();

    return a.exec();
}
