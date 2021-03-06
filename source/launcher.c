#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>

#include "common.h"
#include "fs.h"
#include "gfx.h"
#include "backup.h"
#include "conf.h"
#include "launcher.h"

static Handle nssHandle = 0;

Result launcher_init(){
	Result ret;
	ret = srvGetServiceHandle(&nssHandle, "ns:s");

	return ret;
}

Result launcher_fini(){
	return svcCloseHandle(nssHandle);
}

int prepare_to_launch(char* dirname){
	Handle handle;
	char* savespath = "/TownManager/Saves/";
	char* townpath;
	char* filepath;
	char* buf;
	u64 size;
	u32 read;
	u32 written;
	conftok_t token;
	file_t sdmc_files, game_files;
	int i;
	int error;
	u64 securein;
	u8 secureout;
	Result ret;

	gfx_displaymessage("Preparing to launch game...");
	townpath = calloc(strlen(savespath)+strlen(dirname)+1+1, 1);
	sprintf(townpath, "%s%s/", savespath, dirname);
	sdmc_files = get_files(sdmc_arch, townpath);
	game_files = get_files(game_arch, "/");

	//delete files off of game cart if num of files in townpath > 0
	if(sdmc_files.numfiles > 0){
		for(i = 0; i < game_files.numfiles; i++){
			filepath = calloc(1+strlen(game_files.files[i])+1, 1);
			sprintf(filepath, "/%s", game_files.files[i]);
			gfx_displaymessage("Deleting %s in game archive...", game_files.files[i]);
			ret = FSUSER_DeleteFile(game_arch, fsMakePath(PATH_ASCII, filepath));
			if(ret){
				gfx_error(ret, __FILENAME__, __LINE__);
				return -1;
			}
		}
	}

	//copy local files to cartridge
	for(i = 0; i < sdmc_files.numfiles; i++){
		buf = file_to_buffer(sdmc_arch, townpath, sdmc_files.files[i]);
		if(buf == NULL){
			gfx_waitmessage("Buffer is empty!");
			return -1;
		}
		filepath = calloc(strlen(townpath)+strlen(sdmc_files.files[i])+1, 1);
		sprintf(filepath, "%s%s", townpath, sdmc_files.files[i]);
		size = filesize_to_u64(sdmc_arch, filepath);
		gfx_displaymessage("Writing %s to game archive...", sdmc_files.files[i]);
		error = buffer_to_file(game_arch, buf, size, "/", sdmc_files.files[i]);
		if(error == -1){
			gfx_waitmessage("buffer_to_file failed!");
			return -1;
		}
	}
	if(is3dsx)
		fs_fini();
	securein = ((u64)SECUREVALUE_SLOT_SD << 32) | (uniqueid << 8);
	ret = FSUSER_ControlSecureSave(SECURESAVE_ACTION_DELETE, &securein, 8, &secureout, 1);
	if(ret){
		gfx_error(ret, __FILENAME__, __LINE__);
		return -1;
	}
	if(is3dsx)
		fs_init();
	free(townpath);
	if((ret = FSUSER_OpenFile(&handle, sdmc_arch, fsMakePath(PATH_ASCII, "/TownManager/config"), FS_OPEN_READ | FS_OPEN_WRITE, 0))){
		gfx_error(ret, __FILENAME__, __LINE__);
		return -1;
	}
	if((ret = FSFILE_GetSize(handle, &size))){
		gfx_error(ret, __FILENAME__, __LINE__);
		return -1;
	}
	if((buf = malloc(size)) == NULL){
		gfx_waitmessage("%s:%d malloc failed!", __FILENAME__, __LINE__);
		return -1;
	}
	if((ret = FSFILE_Read(handle, &read, 0, buf, size))){
		gfx_error(ret, __FILENAME__, __LINE__);
		return -1;
	}
	conf_parse(buf, &token);
	token.townname = dirname;
	free(buf);
	buf = calloc(2+strlen(token.townname)+1+1, 1); //1 = '\0'
	conf_gen(&buf, &token);
	if((ret = FSFILE_SetSize(handle, strlen(buf)))){
		gfx_error(ret, __FILENAME__, __LINE__);
		return -1;
	}
	if((ret = FSFILE_Write(handle, &written, 0, buf, strlen(buf), FS_WRITE_FLUSH | FS_WRITE_UPDATE_TIME))){
		gfx_error(ret, __FILENAME__, __LINE__);
		return -1;
	}
	if((ret = FSFILE_Close(handle))){
		gfx_error(ret, __FILENAME__, __LINE__);
		return -1;
	}

	return 0;
}

Result NSS_Reboot(u32 pid_low, u32 pid_high, u8 mediatype, u8 flag){
	Result ret;
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = 0x00100180;
	cmdbuf[1] = flag;
	cmdbuf[2] = pid_low;
	cmdbuf[3] = pid_high;
	cmdbuf[4] = mediatype;
	cmdbuf[5] = 0x00000000;
	cmdbuf[6] = 0x00000000;

	if((ret = svcSendSyncRequest(nssHandle))!=0) return ret;

	return (Result)cmdbuf[1];
}

void launch_game(){
	Result ret;

	if(is3dsx){
		if((ret = NSS_Reboot(lowerid, upperid, mediatype, 0x1))){
			gfx_error(ret, __FILE__, __LINE__);
		}
	}
	else{
		u8 param[0x300];
		u8 hmac[0x20];
		memset(param, 0, sizeof(param));
		memset(hmac, 0, sizeof(hmac));

		APT_PrepareToDoApplicationJump(0, titleid, mediatype);
		APT_DoApplicationJump(param, sizeof(param), hmac);
	}
}
