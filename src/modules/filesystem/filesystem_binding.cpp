/**
* This file has been modified from its orginal sources.
*
* Copyright (c) 2012 Software in the Public Interest Inc (SPI)
* Copyright (c) 2012 David Pratt
* Copyright (c) 2012 Mital Vora
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
***
* Copyright (c) 2008-2012 Appcelerator Inc.
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#include <tideutils/file_utils.h>
#include <tideutils/environment_utils.h>
using namespace TideUtils;

#include <tide/tide.h>
#include <tide/thread_manager.h>
#include "filesystem_binding.h"
#include "file.h"
#include "file_stream.h"
#include "async_copy.h"
#include "filesystem_utils.h"

#ifdef OS_OSX
#include <Cocoa/Cocoa.h>
#elif OS_WIN32
#include <windows.h>
#include <shlobj.h>
#include <process.h>
#elif OS_LINUX
#include <sys/types.h>
#include <pwd.h>
#endif

#include <Poco/TemporaryFile.h>
#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/LineEndingConverter.h>
#include <Poco/Exception.h>

namespace ti
{
    FilesystemBinding::FilesystemBinding(Host *host, TiObjectRef global) :
        StaticBoundObject("Filesystem"),
        host(host),
        global(global),
        timer(0)
    {

        this->SetMethod("createTempFile", &FilesystemBinding::CreateTempFile);
        this->SetMethod("createTempDirectory", &FilesystemBinding::CreateTempDirectory);
        this->SetMethod("getFile", &FilesystemBinding::GetFile);
        this->SetMethod("getFileStream", &FilesystemBinding::GetFileStream);
        this->SetMethod("getProgramsDirectory", &FilesystemBinding::GetProgramsDirectory);
        this->SetMethod("getApplicationDirectory", &FilesystemBinding::GetApplicationDirectory);
        this->SetMethod("getApplicationDataDirectory", &FilesystemBinding::GetApplicationDataDirectory);
        this->SetMethod("getRuntimeHomeDirectory", &FilesystemBinding::GetRuntimeHomeDirectory);
        this->SetMethod("getResourcesDirectory", &FilesystemBinding::GetResourcesDirectory);
        this->SetMethod("getDesktopDirectory", &FilesystemBinding::GetDesktopDirectory);
        this->SetMethod("getDocumentsDirectory", &FilesystemBinding::GetDocumentsDirectory);
        this->SetMethod("getUserDirectory", &FilesystemBinding::GetUserDirectory);
        this->SetMethod("getLineEnding", &FilesystemBinding::GetLineEnding);
        this->SetMethod("getSeparator", &FilesystemBinding::GetSeparator);
        this->SetMethod("getRootDirectories", &FilesystemBinding::GetRootDirectories);
        this->SetMethod("asyncCopy", &FilesystemBinding::ExecuteAsyncCopy);

        this->SetInt("MODE_READ", FileStream::MODE_READ);
        this->SetInt("MODE_WRITE", FileStream::MODE_WRITE);
        this->SetInt("MODE_APPEND", FileStream::MODE_APPEND);
        this->SetInt("SEEK_START", std::ios::beg);
        this->SetInt("SEEK_CURRENT", std::ios::cur);
        this->SetInt("SEEK_END", std::ios::end);
    }

    FilesystemBinding::~FilesystemBinding()
    {
        if (this->timer!=NULL)
        {
            this->timer->stop();
            delete this->timer;
            this->timer = NULL;
        }
    }

    void FilesystemBinding::CreateTempFile(const ValueList& args, ValueRef result)
    {
        try
        {
            Poco::TemporaryFile tempFile;
            tempFile.keepUntilExit();
            tempFile.createFile();

            ti::File* jsFile = new ti::File(tempFile.path());
            result->SetObject(jsFile);
        }
        catch (Poco::Exception& exc)
        {
            throw ValueException::FromString(exc.displayText());
        }
    }

    void FilesystemBinding::CreateTempDirectory(const ValueList& args, ValueRef result)
    {
        try
        {
            Poco::TemporaryFile tempDir;
            tempDir.keepUntilExit();
            tempDir.createDirectory();

            ti::File* jsFile = new ti::File(tempDir.path());
            result->SetObject(jsFile);
        }
        catch (Poco::Exception& exc)
        {
            throw ValueException::FromString(exc.displayText());
        }
    }


    void FilesystemBinding::GetFile(const ValueList& args, ValueRef result)
    {
        result->SetObject(new ti::File(FilesystemUtils::FilenameFromArguments(args)));
    }

    void FilesystemBinding::GetFileStream(const ValueList& args, ValueRef result)
    {
        result->SetObject(new ti::FileStream(FilesystemUtils::FilenameFromArguments(args)));
    }

    void FilesystemBinding::GetApplicationDirectory(const ValueList& args, ValueRef result)
    {
        result->SetObject(new ti::File(host->GetApplication()->path));
    }

    void FilesystemBinding::GetApplicationDataDirectory(const ValueList& args, ValueRef result)
    {
        result->SetObject(new ti::File(
            Host::GetInstance()->GetApplication()->GetDataPath()));
    }

    void FilesystemBinding::GetRuntimeHomeDirectory(const ValueList& args, ValueRef result)
    {
        std::string dir = FileUtils::GetSystemRuntimeHomeDirectory();
        ti::File* file = new ti::File(dir);
        result->SetObject(file);
    }

    void FilesystemBinding::GetResourcesDirectory(const ValueList& args, ValueRef result)
    {
        ti::File* file = new ti::File(host->GetApplication()->GetResourcesPath());
        result->SetObject(file);
    }

    void FilesystemBinding::GetProgramsDirectory(const ValueList &args, ValueRef result)
    {
#ifdef OS_WIN32
        wchar_t path[MAX_PATH];
        if (!SHGetSpecialFolderPathW(NULL, path, CSIDL_PROGRAM_FILES, FALSE))
            throw ValueException::FromString("Could not get Program Files path.");
        std::string dir(::WideToUTF8(path));

#elif OS_OSX
        std::string dir([[NSSearchPathForDirectoriesInDomains(
            NSApplicationDirectory, NSLocalDomainMask, YES)
            objectAtIndex: 0] UTF8String]);

#elif OS_LINUX
        // TODO: this might need to be configurable
        std::string dir("/usr/local/bin");
#endif
        ti::File* file = new ti::File(dir);
        result->SetObject(file);
    }

    void FilesystemBinding::GetDesktopDirectory(const ValueList& args, ValueRef result)
    {
#ifdef OS_WIN32
        wchar_t path[MAX_PATH];
        if (!SHGetSpecialFolderPathW(NULL, path, CSIDL_DESKTOPDIRECTORY, FALSE))
            throw ValueException::FromString("Could not get Desktop path.");
        std::string dir(::WideToUTF8(path));

#elif OS_OSX
        std::string dir([[NSSearchPathForDirectoriesInDomains(
            NSDesktopDirectory, NSUserDomainMask, YES)
            objectAtIndex: 0] UTF8String]);

#elif OS_LINUX
        passwd *user = getpwuid(getuid());
        std::string homeDirectory = user->pw_dir;
        std::string dir(FileUtils::Join(homeDirectory.c_str(), "Desktop", NULL));
        if (!FileUtils::IsDirectory(dir))
        {
            dir = homeDirectory;
        }
#endif
        ti::File* file = new ti::File(dir);
        result->SetObject(file);
    }

    void FilesystemBinding::GetDocumentsDirectory(const ValueList& args, ValueRef result)
    {
#ifdef OS_WIN32
        wchar_t path[MAX_PATH];
        if (!SHGetSpecialFolderPathW(NULL, path, CSIDL_PERSONAL, FALSE))
            throw ValueException::FromString("Could not get Documents path.");
        std::string dir(::WideToUTF8(path));

#elif OS_OSX
        std::string dir([[NSSearchPathForDirectoriesInDomains(
            NSDocumentDirectory, NSUserDomainMask, YES)
            objectAtIndex: 0] UTF8String]);

#elif OS_LINUX
        passwd* user = getpwuid(getuid());
        std::string homeDirectory = user->pw_dir;
        std::string dir(FileUtils::Join(homeDirectory.c_str(), "Documents", NULL));
        if (!FileUtils::IsDirectory(dir))
        {
            dir = homeDirectory;
        }
#endif
        ti::File* file = new ti::File(dir);
        result->SetObject(file);
    }

    void FilesystemBinding::GetUserDirectory(const ValueList& args, ValueRef result)
    {
        std::string dir;
        try
        {
            dir = Poco::Path::home().c_str();
        }
        catch (Poco::Exception& exc)
        {
            std::string error = "Could not determine home directory: ";
            error.append(exc.displayText());
            throw ValueException::FromString(error);
        }

#ifdef OS_WIN32
        // If dir is equal to C:\ get the diretory in a different way.
        // Poco uses %%HOMEPATH%% to get this directory which might be borked
        // e.g. while running in Cygwin.
        if (dir.size() == 3)
        {
            std::string odir = EnvironmentUtils::Get("USERPROFILE");
            if (!odir.empty())
            {
                dir = odir;
            }
        }
#endif

        ti::File* file = new ti::File(dir);
        result->SetObject(file);
    }

    void FilesystemBinding::GetLineEnding(const ValueList& args, ValueRef result)
    {
        try
        {
            result->SetString(Poco::LineEnding::NEWLINE_LF.c_str());
        }
        catch (Poco::Exception& exc)
        {
            throw ValueException::FromString(exc.displayText());
        }
    }

    void FilesystemBinding::GetSeparator(const ValueList& args, ValueRef result)
    {
        try
        {
            std::string sep;
            sep += Poco::Path::separator();
            result->SetString(sep.c_str());
        }
        catch (Poco::Exception& exc)
        {
            throw ValueException::FromString(exc.displayText());
        }
    }

    void FilesystemBinding::GetRootDirectories(const ValueList& args, ValueRef result)
    {
        try
        {
            Poco::Path path;
            std::vector<std::string> roots;
            path.listRoots(roots);

            TiListRef rootList = new StaticBoundList();
            for(size_t i = 0; i < roots.size(); i++)
            {
                ti::File* file = new ti::File(roots.at(i));
                ValueRef value = Value::NewObject((TiObjectRef) file);
                rootList->Append(value);
            }

            TiListRef list = rootList;
            result->SetList(list);
        }
        catch (Poco::Exception& exc)
        {
            throw ValueException::FromString(exc.displayText());
        }
    }

    void FilesystemBinding::ExecuteAsyncCopy(const ValueList& args, ValueRef result)
    {
        if (args.size()!=3)
        {
            throw ValueException::FromString("invalid arguments - this method takes 3 arguments");
        }
        std::vector<std::string> files;
        if (args.at(0)->IsString())
        {
            files.push_back(args.at(0)->ToString());
        }
        else if (args.at(0)->IsList())
        {
            TiListRef list = args.at(0)->ToList();
            for (unsigned int c = 0; c < list->Size(); c++)
            {
                files.push_back(FilesystemUtils::FilenameFromValue(list->At(c)));
            }
        }
        else
        {
            files.push_back(FilesystemUtils::FilenameFromValue(args.at(0)));
        }
        ValueRef v = args.at(1);
        std::string destination(FilesystemUtils::FilenameFromValue(v));
        TiMethodRef method = args.at(2)->ToMethod();
        TiObjectRef copier = new ti::AsyncCopy(this,host,files,destination,method);
        result->SetObject(copier);
        asyncOperations.push_back(copier);
        // we need to create a timer thread that can cleanup operations
        if (timer==NULL)
        {
            this->SetMethod("_invoke",&FilesystemBinding::DeletePendingOperations);
            timer = new Poco::Timer(100,100);
            Poco::TimerCallback<FilesystemBinding> cb(*this, &FilesystemBinding::OnAsyncOperationTimer);
            timer->start(cb);
        }
        else
        {
            this->timer->restart(100);
        }
    }

    void FilesystemBinding::DeletePendingOperations(const ValueList& args, ValueRef result)
    {
        TIDE_DUMP_LOCATION
        if (asyncOperations.size()==0)
        {
            result->SetBool(true);
            return;
        }
        std::vector<TiObjectRef>::iterator iter = asyncOperations.begin();

        while (iter!=asyncOperations.end())
        {
            TiObjectRef c = (*iter);
            ValueRef v = c->Get("running");
            bool running = v->ToBool();
            if (!running)
            {
                asyncOperations.erase(iter);
                break;
            }
            iter++;
        }

        // return true to pause the timer
        result->SetBool(asyncOperations.size()==0);
    }

    void FilesystemBinding::OnAsyncOperationTimer(Poco::Timer &timer)
    {
        START_TIDE_THREAD;

        ValueList args = ValueList();
        TiMethodRef m = this->Get("_invoke")->ToMethod();
        ValueRef result = RunOnMainThread(m, args);
        if (result->ToBool())
        {
            timer.restart(0);
        }

        END_TIDE_THREAD;
    }
}
