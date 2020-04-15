#include "PackageDownloader.h"
#include "utils.h"
#include "Container.h"
#include <fstream>
#include <cassert>

PackageDownloader::PackageDownloader(EventQueue* eq, int packageStartIdx, int packageEndIdx)
    : ChecksumTracker()
    , eq(eq)
    , numPackages(packageEndIdx - packageStartIdx + 1)
    , packageStartIdx(packageStartIdx)
    , packageEndIdx(packageEndIdx)
{
    #ifdef DEBUG
    std::unique_lock<std::mutex> stdoutLock(eq->stdoutMutex);
    printf("PackageDownloader n:%d s:%d e:%d\n", numPackages, packageStartIdx, packageEndIdx);
    #endif
}

PackageDownloader::~PackageDownloader() {
    // nop
}

#ifdef USE_FAST
    void PackageDownloader::run() {
        Container<std::string> packages = readFile("data/packages.txt");
        int counter = packageStartIdx;
        while (counter <= packageEndIdx) {
            std::string packageName = packages[counter % packages.size()];
            eq->enqueueEvent(Event(Event::DOWNLOAD_COMPLETE, new Package(packageName)));
            updateGlobalChecksum(sha256(packageName));
            counter++;
        }
    }
#else
    void PackageDownloader::run() {
        for (int i = packageStartIdx; i <= packageEndIdx; i++) {
            std::string packageName = readFileLine("data/packages.txt", i);

            eq->enqueueEvent(Event(Event::DOWNLOAD_COMPLETE, new Package(packageName)));
            updateGlobalChecksum(sha256(packageName));
        }
    }
#endif
