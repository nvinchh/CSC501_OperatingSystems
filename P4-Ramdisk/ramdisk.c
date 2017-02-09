

#define FUSE_USE_VERSION 26

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define SIZE 255

typedef struct rmfsDirNode
{
	char dirNodeName[SIZE];
	struct stat *dirNodeInfo;
	struct rmfsDirNode *parent, *child, *sibling;
	char *data, nodeType;
}Node;

Node *rootNode;
char tempFName[SIZE];
long fSize;

Node* lookupPath(const char *path, int flag)
{
	if(strcmp(path, "/") == 0)
		return rootNode;

	char *tokenPath, prevToken[SIZE], savePath[PATH_MAX];
	strcpy(savePath, path);
	tokenPath = strtok(savePath, "/");

	Node *rootPtr = rootNode, *temp = NULL;
	int found = 0;

	while(tokenPath != NULL)
	{
		//printf("token %s\n",tokenPath );
		for(temp = rootPtr->child; temp != NULL; temp = temp->sibling)
		{
			if(strcmp(temp->dirNodeName, tokenPath) == 0)
			{
				found = 1;
				break;
			}
		}

		strcpy(prevToken, tokenPath);
		tokenPath = strtok(NULL, "/");
		if(found == 1)
		{
			if(tokenPath == NULL)
				return temp;
		}
		else
		{
			if(flag == 1)
			{
				strcpy(tempFName, prevToken);
				return rootPtr;
			}
			return NULL;
		}
		rootPtr = temp;
		found = 0;
	}
	return NULL;
}

static int rmfsCreate(const char *path, mode_t mode, struct fuse_file_info *fInfo)
{
	//printf("Vinchhi ramdisk create\n");
	Node *lookupNode = lookupPath(path, 1);
	if(lookupNode == NULL)
		return -ENOENT;

	Node *newFile = (Node *) malloc(sizeof(Node));
	if(fSize <= 0 || newFile == NULL)
		return -ENOMEM;

	newFile->dirNodeInfo = (struct stat *)malloc(sizeof(struct stat));
	strcpy(newFile->dirNodeName, tempFName);
	newFile->dirNodeInfo->st_mode = S_IFREG | 0755;
	newFile->parent = lookupNode;
	newFile->child = NULL;
	newFile->sibling = NULL;
	newFile->data = NULL;
	newFile->dirNodeInfo->st_nlink = 1;
	newFile->dirNodeInfo->st_size = 0;
	newFile->nodeType = 'F';

	if(lookupNode->child == NULL)
		lookupNode->child = newFile;
	else
	{
		Node *temp = lookupNode->child;
		while(temp->sibling != NULL)
			temp = temp->sibling;
		temp->sibling = newFile;
	}
	fSize = fSize - sizeof(Node) - sizeof(struct stat);

	return 0;
}

static int rmfsGetAttr(const char *path, struct stat *stBuffer)
{
	memset(stBuffer, 0, sizeof(struct stat));

	if(strcmp(path, "/") == 0)
	{
		stBuffer->st_mode = S_IFDIR | 0755;
		stBuffer->st_nlink = 2;
	}
	else
	{
		Node *lookupNode = lookupPath(path, 0);

		if(lookupNode == NULL)
			return -ENOENT;
		else
		{
			stBuffer->st_mode = lookupNode->dirNodeInfo->st_mode;
			stBuffer->st_nlink = lookupNode->dirNodeInfo->st_nlink;
			stBuffer->st_size = lookupNode->dirNodeInfo->st_size;
		}
	}
	return 0;
}

static int rmfsMkDir(const char *path, mode_t mode)
{
	//printf("Vinchhi ramdisk make directory\n");
	Node *temp = lookupPath(path, 1);
	Node *newN = (Node *) malloc(sizeof(Node));

	newN->dirNodeInfo = (struct stat*) malloc(sizeof(struct stat));
	if(newN == NULL)
		return -ENOSPC;

	fSize = fSize - sizeof(Node) - sizeof(struct stat);

	if(fSize < 0)
		return -ENOSPC;

	strcpy(newN->dirNodeName, tempFName);

	newN->dirNodeInfo->st_mode = S_IFDIR | 0755;
	newN->dirNodeInfo->st_nlink = 2;
	newN->parent = temp;
	newN->sibling = NULL;
	newN->dirNodeInfo->st_size = 4096;
	newN->nodeType = 'D';

	if(temp->child == NULL)
	{
		temp->child = newN;
		temp->dirNodeInfo->st_nlink += 1;
	}
	else
	{
		Node *tempChild = temp->child;
		while(tempChild->sibling != NULL)
			tempChild = tempChild->sibling;

		tempChild->sibling = newN;
		temp->dirNodeInfo->st_nlink += 1;
	}
	return 0;
}

static int rmfsOpen(const char *path, struct fuse_file_info *fInfo)
{
	//printf("Vinchhi ramdisk open\n");
	Node *lookupNode = lookupPath(path, 0);

	if(lookupNode != NULL)
		return 0;
	else
		return -ENOENT;
}

static int rmfsOpenDir(const char *path, struct fuse_file_info *fInfo)
{
	//printf("Vinchhi ramdisk open directory\n");
	Node *lookupNode = lookupPath(path, 0);
	if(lookupNode != NULL)
		return 0;
	else
		return -ENOENT;
}

static int rmfsRead(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fInfo)
{
	//printf("Vinchhi ramdisk read\n");
	Node *lookupNode = lookupPath(path, 0);

	if(lookupNode == NULL)
		return -ENOENT;

	if(lookupNode->nodeType == 'D')
		return -EISDIR;

	size_t length;
	length = lookupNode->dirNodeInfo->st_size;

	if(offset < length)
	{
		if(offset + size > length)
			size = length - offset;
		memcpy(buffer, lookupNode->data + offset, size);
	}
	else
		size = 0;

	return size;
}

static int rmfsReadDir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fInfo)
{
	//printf("Vinchhi ramdisk readdir\n");
	Node *lookupNode = lookupPath(path, 1);

	if(lookupNode == NULL)
		return -ENOENT;

	filler(buffer, ".", NULL, 0);
	filler(buffer, "..", NULL, 0);

	Node *temp;

	for(temp = lookupNode->child; temp != NULL; temp = temp->sibling)
	{
		filler(buffer, temp->dirNodeName, NULL, 0);
	}
	return 0;
}

static int rmfsUnlink(const char *path)
{
	//printf("Vinchhi ramdisk unlink\n");
	Node* lookupNode = lookupPath(path, 0);

	if(lookupNode == NULL)
		return -ENOENT;
	else
	{
		//don't remove if the given directory is not empty
		//Node * parent=nodeLookedup->parent;
			// if child node of parent
		if(lookupNode->parent->child == lookupNode)
			lookupNode->parent->child = lookupNode->sibling;
		else
		{
			Node *find = lookupNode->parent->child;
			while(find != NULL && find->sibling != lookupNode)
				find = find->sibling;
			find->sibling = lookupNode->sibling;
		}

		fSize = fSize + sizeof(struct stat) + sizeof(Node) + sizeof(lookupNode->dirNodeInfo->st_size);
		free(lookupNode->data);
		free(lookupNode->dirNodeInfo);
		free(lookupNode);
		return 0;
	}
	return -ENOENT;
}

static int rmfsRename(const char *path, const char *newPath)
{
	//printf("Vinchhi ramdisk rename\n");
	Node *oldN = lookupPath(path, 1);
	Node *newN = lookupPath(newPath, 1);

	if(oldN != NULL && newN != NULL)
	{
		if(newN->nodeType == 'D')
		{
			rmfsCreate(newPath, oldN->dirNodeInfo->st_mode, NULL);
			newN = lookupPath(newPath, 1);

			newN->dirNodeInfo->st_mode = oldN->dirNodeInfo->st_mode;
			newN->dirNodeInfo->st_nlink = oldN->dirNodeInfo->st_nlink;
			newN->dirNodeInfo->st_size = oldN->dirNodeInfo->st_size;

			memset(newN->data, 0, newN->dirNodeInfo->st_size);
			if(oldN->dirNodeInfo->st_size > 0)
			{
				newN->data = (char *) realloc(newN->data, sizeof(char) * oldN->dirNodeInfo->st_size);
				if(newN->data == NULL)
					return -ENOSPC;
				strcpy(newN->data, oldN->data);
			}
			newN->dirNodeInfo->st_size = oldN->dirNodeInfo->st_size;
			rmfsUnlink(path);
			return 0;
		}
		else if(newN->nodeType == 'F')
		{
			memset(oldN->dirNodeName, 0, SIZE);
			strcpy(oldN->dirNodeName, newN->dirNodeName);
			return 0;
		}

	}
	else
		return -ENOENT;

	return 0;
}

static int rmfsRmDir(const char *path)
{
	//printf("Vinchhi ramdisk rmdir\n");
	Node* lookupNode = lookupPath(path, 0);

	if(lookupNode == NULL)
		return -ENOENT;

	else
	{
		if(lookupNode->child != NULL)
			return -ENOTEMPTY;

		else
		{
			if(lookupNode->parent->child == lookupNode)
				lookupNode->parent->child = lookupNode->sibling;
			else
			{
				Node* findingNode = lookupNode->parent->child;
				while(findingNode->sibling != lookupNode)
					findingNode = findingNode->sibling;

				findingNode->sibling = lookupNode->sibling;
			}
			lookupNode->parent->dirNodeInfo->st_nlink -= 1;
			free(lookupNode->dirNodeInfo);
			free(lookupNode);

			fSize = fSize + sizeof(struct stat) + sizeof(Node);
			return 0;
		}
	}
	return -ENOENT;
}


static int rmfsUtime(const char *path, struct utimbuf *ubuf)
{
	return 0;
}

static int rmfsWrite(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fInfo)
{
	//printf("Vinchhi ramdisk write\n");
	Node* lookupNode = lookupPath(path, 0);

	//checking whether path valid or not
	if(lookupNode == NULL)
		return -ENOENT;

	// checking wheteher its file or directory
	if(lookupNode->nodeType == 'D')
		return -EISDIR;

	//checking whether space is available or not
	if(fSize < size)
		return -ENOSPC;

	size_t length;
	length = lookupNode->dirNodeInfo->st_size;

	//When writing for the first time
	if(size > 0 && length == 0)
	{
		lookupNode->data = (char *) malloc(sizeof(char) * size);
		offset = 0;
		memcpy(lookupNode->data+offset, buffer, size);
		lookupNode->dirNodeInfo->st_size = offset+size;
		fSize = fSize - size;
		return size;
	}
	else if(size > 0)
	{
		if(offset > length)
		{
			offset = length;
		}
			lookupNode->data = (char *) realloc(lookupNode->data, sizeof(char) * (offset+size));
			memcpy(lookupNode->data+offset, buffer, size);
			lookupNode->dirNodeInfo->st_size = offset+size;
			fSize = fSize - size;
			return size;
	}
	return 0;
}

// mapping fuse structure to the various function implementations
static struct fuse_operations rmfsOper = {
	.create 	= rmfsCreate,
	.getattr 	= rmfsGetAttr,
	.mkdir		= rmfsMkDir,
	.open		= rmfsOpen,
	.opendir	= rmfsOpenDir,
	.read		= rmfsRead,
	.readdir	= rmfsReadDir,
	.rename		= rmfsRename,
	.rmdir		= rmfsRmDir,
	.unlink		= rmfsUnlink,
	.utime		= rmfsUtime,
	.write		= rmfsWrite,
};

int main(int argc, char *argv[])
{
	if(argc!=3)
	{
		printf("Invalid # of args\nSyntax: ramdisk /path/to/dir <size>\n");
		return 0;
	}

	fSize = ((long) atoi(argv[2])) *1024*1024;
	argc--;

	rootNode = (Node *) malloc(sizeof(Node));
	strcpy(rootNode->dirNodeName, "/");

	rootNode->parent = NULL;
	rootNode->child = NULL;
	rootNode->sibling = NULL;

	rootNode->nodeType = 'D';
	rootNode->data = NULL;
	rootNode->dirNodeInfo = (struct stat *) malloc(sizeof(struct stat));
	// initialize stat structure
	// all parameters are not utilized

	rootNode->dirNodeInfo->st_mode = S_IFDIR | 0755;
	rootNode->dirNodeInfo->st_nlink = 2;

	fSize = fSize - sizeof(Node) - sizeof(struct stat);

	return fuse_main(argc, argv, &rmfsOper, NULL);
}