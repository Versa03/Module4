# LawakFS++ - A Cursed Filesystem with Censorship and Strict Access Policies

Teja, a frustrated football fan, is fed up with his favorite team's constant losses, often calling them a "joke team" ("tim lawak"). This disappointment inspired the idea of a special filesystem—LawakFS++—designed to censor "lawak" (ridiculous) content. LawakFS++ is a read-only, cursed filesystem that enforces strict access policies, dynamic content filtering, and time-based access control. It also includes features like logging and configuration management to regulate file access behavior.

- You may choose any source directory and mount point for your filesystem.

- You are **required** to implement at least the following functions within your `fuse_operations` struct:

  - `getattr`
  - `readdir`
  - `read`
  - `open`
  - `access`

- You are permitted to include additional functions like `init`, `destroy`, or `readlink` if they are necessary for your implementation.

- **LawakFS++ must be strictly read-only.** Any attempt to perform write-related operations within the FUSE mountpoint must **fail** and return the `EROFS` (Read-Only File System) error.

- The following system calls, and thus commands that rely on them, must be explicitly blocked:

  - `write()`
  - `truncate()`
  - `create()`
  - `unlink()`
  - `mkdir()`
  - `rmdir()`
  - `rename()`

> **Note:** When users attempt to use commands such as `touch`, `rm`, `mv`, or any other command that performs a write operation, they should receive a clear "Permission denied" or "Read-only file system" error.

### a. Hidden File Extensions

After using regular filesystems for days, Teja realized that file extensions always made it too easy for people to identify file types. "This is too predictable!" he thought. He wanted to create a more mysterious system where people would have to actually open files to discover their contents.

All files displayed within the FUSE mountpoint must have their **extensions hidden**.

- **Example:** If the original file is `document.pdf`, an `ls` command inside the FUSE directory should only show `document`.
- **Behavior:** Despite the hidden extension, accessing a file (e.g., `cat /mnt/your_mountpoint/document`) must correctly map to its original path and name (e.g., `source_dir/document.pdf`).

### b. Time-Based Access for Secret Files

One day, Teja discovered his collection of embarrassing high school photos stored in a folder named "secret". He didn't want others to access these files at any time, especially when he was sleeping or away from home. "Secret files should only be accessible during working hours!" he decided firmly.

Files whose base name is **`secret`** (e.g., `secret.txt`, `secret.zip`) can only be accessed **between 08:00 (8 AM) and 18:00 (6 PM) system time**.

- **Restriction:** Outside this specified time range, any attempt to open, read, or even list the `secret` file must result in an `ENOENT` (No such file or directory) error.
- **Hint:** You will need to implement this time-based access control within your `access()` and/or `getattr()` FUSE operations.

### c. Dynamic Content Filtering

Teja's frustration with "lawak" things reached its peak when he read online articles full of words that annoyed him. Not only that, but the images he viewed often didn't meet his expectations either. "All content entering my system must be filtered first!" he exclaimed while clenching his fist.

When a file is opened and read, its content must be **dynamically filtered or transformed** based on its perceived type:

| File Type        | Behavior                                                                                      |
| :--------------- | :-------------------------------------------------------------------------------------------- |
| **Text Files**   | All words considered lawak (case-insensitive) must be replaced with the word `"lawak"`.       |
| **Binary Files** | The raw binary content must be presented in **Base64 encoding** instead of its original form. |

> **Note:** The list of "lawak words" for text file filtering will be defined externally, as specified in requirement **e. Configuration**.

### d. Access Logging

As a paranoid individual, Teja felt the need to record every activity happening in his filesystem. "Who knows if someone tries to access my important files without permission," he muttered while setting up the logging system. He wanted every movement recorded in detail, complete with timestamps and the perpetrator's identity.

All file access operations performed within LawakFS++ must be **logged** to a file located at **`/var/log/lawakfs.log`**.

Each log entry must strictly adhere to the following format:

```
[YYYY-MM-DD HH:MM:SS] [UID] [ACTION] [PATH]
```

Where:

- **`YYYY-MM-DD HH:MM:SS`**: The timestamp of the operation.
- **`UID`**: The User ID of the user performing the action.
- **`ACTION`**: The type of FUSE operation (e.g., `READ`, `ACCESS`, `GETATTR`, `OPEN`, `READDIR`).
- **`PATH`**: The logical path to the file or directory within the FUSE mountpoint (e.g., `/secret`, `/images/photo.jpg`).

> **Requirement:** You are **only required** to log successful `read` and `access` operations. Logging of other operations (e.g., failed writes) is optional.

### e. Configuration

After using his filesystem for several weeks, Teja realized that his needs kept changing. Sometimes he wanted to add new words to the filter list, sometimes he wanted to change the secret file access hours, or even change the name of the secret file itself. "I don't want the hassle of recompiling every time I want to change settings!" he complained. Finally, he decided to create a flexible external configuration system.

To ensure flexibility, the following parameters **must not be hardcoded** within your `lawak.c` source code. Instead, they must be configurable via an external configuration file (e.g., `lawak.conf`):

- The **base filename** of the 'secret' file (e.g., `secret`).
- The **start time** for accessing the 'secret' file.
- The **end time** for accessing the 'secret' file.
- The comma-separated **list of words to be filtered** from text files.

**Example `lawak.conf` content:**

```
FILTER_WORDS=ducati,ferrari,mu,chelsea,prx,onic,sisop
SECRET_FILE_BASENAME=secret
ACCESS_START=08:00
ACCESS_END=18:00
```

Your FUSE should read and parse this configuration file upon initialization.

### Summary of Expected Behaviors

To ensure clarity, here's a consolidated table of the expected behavior for specific scenarios:

| Scenario                                                    | Expected Behavior                                                                           |
| :---------------------------------------------------------- | :------------------------------------------------------------------------------------------ |
| Accessing a file outside allowed time (e.g., `secret` file) | Return `ENOENT` (No such file or directory)                                                 |
| Reading a binary file                                       | Content must be outputted in **Base64 encoding**                                            |
| Reading a text file                                         | Filtered words must be replaced with `"lawak"`                                              |
| Listing files in any directory                              | All file extensions must be hidden                                                          |
| Attempting to write, create, or rename any file/directory   | Return `EROFS` (Read-Only File System)                                                      |
| Logging of file operations                                  | A new entry must be added to `/var/log/lawakfs.log` for each `read` and `access` operation. |

### Example Behavior

```bash
$ ls /mnt/lawak/
secret   image   readme

$ cat /mnt/lawak/secret
cat: /mnt/lawak/secret: No such file or directory
# (This output is expected if accessed outside 08:00-18:00)

$ cat /mnt/lawak/image
<base64 string of image content>

$ cat /mnt/lawak/readme
"This is a lawak filesystem."
# (Original "sisop" word was replaced with "lawak")

$ sudo tail /var/log/lawakfs.log
[2025-06-10 14:01:22] [1000] [READ] /readme
[2025-06-10 14:01:24] [1000] [ACCESS] /secret
```

