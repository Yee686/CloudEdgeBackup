/*
 * Routines only used by the sending process.
 *
 * Copyright (C) 1996 Andrew Tridgell
 * Copyright (C) 1996 Paul Mackerras
 * Copyright (C) 2003-2018 Wayne Davison
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, visit the http://fsf.org website.
 */

#include "rsync.h"
#include "inums.h"
extern int whole_file;

extern int do_xfers;
extern int am_server;
extern int am_daemon;
extern int inc_recurse;
extern int log_before_transfer;
extern int stdout_format_has_i;
extern int logfile_format_has_i;
extern int want_xattr_optim;
extern int csum_length;
extern int append_mode;
extern int io_error;
extern int flist_eof;
extern int allowed_lull;
extern int preserve_xattrs;
extern int protocol_version;
extern int remove_source_files;
extern int updating_basis_file;
extern int make_backups;
extern int inplace;
extern int batch_fd;
extern int write_batch;
extern int file_old_total;
extern struct stats stats;
extern struct file_list *cur_flist, *first_flist, *dir_flist;

extern int source_is_remote_or_local;  // 0: local, 1: remote 相较于client客户端而言
int task_type_backup_or_recovery_sender = -1; // 0: backup, 1: recovery
extern char *recovery_version;


BOOL extra_flist_sending_enabled;

/**
 * @file
 *
 * The sender gets checksums from the generator, calculates deltas,
 * and transmits them to the receiver.  The sender process runs on the
 * machine holding the source files.
 **/

/**
 * Receive the checksums for a buffer
 **/
static struct sum_struct *receive_sums(int f)
{
	// Receive the checksum sent to the buffer by the generate end. The return value s is the checksum collection
	struct sum_struct *s;
	int32 i;
	int lull_mod = protocol_version >= 31 ? 0 : allowed_lull * 5;
	OFF_T offset = 0;

	if (!(s = new(struct sum_struct)))
		out_of_memory("receive_sums");

	read_sum_head(f, s);

	s->sums = NULL;

	if (DEBUG_GTE(DELTASUM, 3)) {
		rprintf(FINFO, "count=%s n=%ld rem=%ld\n",
			big_num(s->count), (long)s->blength, (long)s->remainder);
	}

	if (append_mode > 0) {
		s->flength = (OFF_T)s->count * s->blength;
		if (s->remainder)
			s->flength -= s->blength - s->remainder;
		return s;
	}

	if (s->count == 0)
		return(s);

	if (!(s->sums = new_array(struct sum_buf, s->count)))
		out_of_memory("receive_sums");

	for (i = 0; i < s->count; i++) {
		s->sums[i].sum1 = read_int(f);
		read_buf(f, s->sums[i].sum2, s->s2length);

		s->sums[i].offset = offset;
		s->sums[i].flags = 0;

		if (i == s->count-1 && s->remainder != 0)
			s->sums[i].len = s->remainder;
		else
			s->sums[i].len = s->blength;
		offset += s->sums[i].len;

		if (lull_mod && !(i % lull_mod))
			maybe_send_keepalive(time(NULL), True);

		if (DEBUG_GTE(DELTASUM, 3)) {
			rprintf(FINFO,
				"chunk[%d] len=%d offset=%s sum1=%08x\n",
				i, s->sums[i].len, big_num(s->sums[i].offset),
				s->sums[i].sum1);
		}
	}

	s->flength = offset;

	return s;
}

void successful_send(int ndx)
{
	char fname[MAXPATHLEN];
	char *failed_op;
	struct file_struct *file;
	struct file_list *flist;
	STRUCT_STAT st;

	if (!remove_source_files)
		return;

	flist = flist_for_ndx(ndx, "successful_send");
	file = flist->files[ndx - flist->ndx_start];
	if (!change_pathname(file, NULL, 0))
		return;
	f_name(file, fname);

	if (do_lstat(fname, &st) < 0) {
		failed_op = "re-lstat";
		goto failed;
	}

	if (S_ISREG(file->mode) /* Symlinks & devices don't need this check: */
	 && (st.st_size != F_LENGTH(file) || st.st_mtime != file->modtime
#ifdef ST_MTIME_NSEC
	 || (NSEC_BUMP(file) && (uint32)st.ST_MTIME_NSEC != F_MOD_NSEC(file))
#endif
	)) {
		rprintf(FERROR_XFER, "ERROR: Skipping sender remove for changed file: %s\n", fname);
		return;
	}

	if (do_unlink(fname) < 0) {
		failed_op = "remove";
	  failed:
		if (errno == ENOENT)
			rprintf(FINFO, "sender file already removed: %s\n", fname);
		else
			rsyserr(FERROR_XFER, errno, "sender failed to %s %s", failed_op, fname);
	} else {
		if (INFO_GTE(REMOVE, 1))
			rprintf(FINFO, "sender removed %s\n", fname);
	}
}

static void write_ndx_and_attrs(int f_out, int ndx, int iflags,
				const char *fname, struct file_struct *file,
				uchar fnamecmp_type, char *buf, int len)
{
	write_ndx(f_out, ndx);
	if (protocol_version < 29)
		return;
	write_shortint(f_out, iflags);
	if (iflags & ITEM_BASIS_TYPE_FOLLOWS)
		write_byte(f_out, fnamecmp_type);
	if (iflags & ITEM_XNAME_FOLLOWS)
		write_vstring(f_out, buf, len);
#ifdef SUPPORT_XATTRS
	if (preserve_xattrs && iflags & ITEM_REPORT_XATTR && do_xfers
	 && !(want_xattr_optim && BITS_SET(iflags, ITEM_XNAME_FOLLOWS|ITEM_LOCAL_CHANGE)))
		send_xattr_request(fname, file, f_out);
#endif
}

int compare_delta_file_name(const void *a, const void *b)
{
	return strcmp(*(char **)a, *(char **)b);
}

void extract_file_name_timestamp(const char *file_name, char *timestamp)
{
	char *timestamp_start = strrchr(file_name, '.');	// xxxx.full(delta).xxxx-xx-xx-xx:xx:xx

	if (timestamp_start != NULL ) {
		// rprintf(FWARNING, "[yee-%s] sender.c: extract_file_name_timestamp {%s} time = {%s}\n", who_am_i(), file_name, timestamp_start+1);
		strcpy(timestamp, timestamp_start+1);
	}
	else 
	{
		rprintf(FWARNING, "[yee-%s] sender.c: extract_file_name_timestamp fname{%s} is illegal\n", who_am_i(), file_name);
	}
}

// TODO: 已将xxxx_fname全换成了xxxx_fpath 需要删除冗余的代码
int make_delta_to_full(const char* fname, const char* recovery_timestamp)
{
	char cwd[MAXPATHLEN];

	rprintf(FWARNING, "[yee-%s] sender.c: make_delta_to_full fname: %s time: %s\n", who_am_i(), fname, recovery_timestamp);

	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		rprintf(FWARNING, "[yee-%s] sender.c: make_delta_to_full current working directory: %s\n", who_am_i(), cwd);
	} else {
		rprintf(FWARNING, "[yee-%s] sender.c: make_delta_to_full getcwd() error\n", who_am_i());
	}

	// fname可能是子目录下的文件 需要分割出目录名和文件名
	char *ptr = strrchr(fname, '/');
	char dir_name[MAXPATHLEN];
	char file_name[MAXPATHLEN];

	if(ptr != NULL)
	{
		strncpy(dir_name, fname, ptr - fname);
		dir_name[ptr - fname] = '\0';
		strcpy(file_name, ptr + 1);
	}
	else
	{
		strcpy(dir_name, ".");
		strcpy(file_name, fname);
	}

	rprintf(FWARNING, "[yee-%s] sender.c: make_delta_to_full dir_name: %s file_name: %s\n", who_am_i(), dir_name, file_name);

	char backup_path[MAXPATHLEN];
	strcpy(backup_path, dir_name);
	strcat(backup_path, "/");
	strcat(backup_path, file_name);
	strcat(backup_path, ".backup/");


	// 打开备份文件路径
	DIR *dir = opendir(backup_path);
	if ( dir )
	{
		// rprintf(FWARNING, "[yee-%s] sender.c: make_delta_to_full opendir() success\n", who_am_i());
		char full_fname[MAXPATHLEN];
		char full_fpath[MAXPATHLEN];
		char full_fname_prefix[MAXPATHLEN];
		int full_count = 0;

		char delta_fname[100][MAXPATHLEN];
		char delta_fpath[100][MAXPATHLEN];
		char delta_fname_prefix[MAXPATHLEN];
		int delta_count = 0;
		int delta_count_in_range = 0;

		strcpy(full_fname_prefix, file_name);
		strcat(full_fname_prefix, ".full.");
		// rprintf(FWARNING, "[yee-%s] sender.c: make_delta_to_full full_fname_prefix: %s\n", who_am_i(), full_fname_prefix);

		strcpy(delta_fname_prefix, file_name);
		strcat(delta_fname_prefix, ".delta.");
		// rprintf(FWARNING, "[yee-%s] sender.c: make_delta_to_full delta_fname_prefix: %s\n", who_am_i(), delta_fname_prefix);

		// 在当前路径下 找到对应的full_file_name文件和delta文件
		struct dirent *ent;
		while((ent = readdir(dir)) != NULL)
		{
			// rprintf(FWARNING, "[yee-%s] sender.c: make_delta_to_full ent->d_name: %s\n", who_am_i(), ent->d_name);
			if(full_count ==0 && strncmp(ent->d_name, full_fname_prefix, strlen(full_fname_prefix)) == 0)
			{
				strcpy(full_fname, ent->d_name);
				
				strcpy(full_fpath, dir_name);
				strcat(full_fpath, "/");
				strcat(full_fpath, file_name);
				strcat(full_fpath, ".backup/");
				strcat(full_fpath, full_fname);

				full_count++;
			}
			else if(strncmp(ent->d_name, delta_fname_prefix, strlen(delta_fname_prefix)) == 0)
			{
				// rprintf(FWARNING, "[yee-%s] sender.c: make_delta_to_full delta_fname[%d]: %s\n", who_am_i(), delta_count, ent->d_name);
				strcpy(delta_fname[delta_count], ent->d_name);

				strcpy(delta_fpath[delta_count], dir_name);
				strcat(delta_fpath[delta_count], "/");
				strcat(delta_fpath[delta_count], file_name);
				strcat(delta_fpath[delta_count], ".backup/");

				strcat(delta_fpath[delta_count], delta_fname[delta_count]);

				delta_count++;
			}
		}

		rprintf(FWARNING, "[yee-%s] sender.c: make_delta_to_full full_count: %d full_fname: %s\n", who_am_i(), full_count, full_fname);
		// rprintf(FWARNING, "[yee-%s] sender.c: make_delta_to_full delta_count: %d\n", who_am_i(), delta_count);
		
		if(full_count == 0)
		{
			rprintf(FWARNING, "[yee-%s] sender.c: make_delta_to_full hasn't find [%s]'s full backup\n", who_am_i(), fname);

			return 0;
		}

		if(delta_count == 0)
		{
			char recovery_fname[MAXPATHLEN], recovery_fpath[MAXPATHLEN];
			strcpy(recovery_fname, file_name);
			strcat(recovery_fname, ".recovery.tmp1");

			strcpy(recovery_fpath, dir_name);
			strcat(recovery_fpath, "/");
			strcat(recovery_fpath, file_name);
			strcat(recovery_fpath, ".backup/");
			strcat(recovery_fpath, recovery_fname);

			// rprintf(FWARNING, "[yee-%s] sender.c: make_delta_to_full delta_count = 0, copy full_fname: %s to recovery_fname: %s\n", who_am_i(), full_fpath, recovery_fpath);
			
			if(copy_file(full_fpath, recovery_fpath, -1, 0666) != 0)
			{
				rprintf(FWARNING, "[yee-%s] sender.c: make_delta_to_full delta_count is 0 copy_full_file error\n", who_am_i());
			}
			
			return 0;
		}

		char full_file_timestamp[100];
		extract_file_name_timestamp(full_fname, full_file_timestamp);
		rprintf(FWARNING, "[yee-%s] sender.c: make_delta_to_full full_file_timestamp: %s\n", who_am_i(), full_file_timestamp);

		// 按照文件名的时间时间戳，找到所需要的delta文件
		char delta_file_timestamp[100];
		const char* delta_name_ptrs[100];
		const char* delta_path_ptrs[100];
		for (int i = 0; i < delta_count; i++) {
			extract_file_name_timestamp(delta_fname[i], delta_file_timestamp);
			// rprintf(FWARNING, "[yee-%s] sender.c: make_delta_to_full {%s} time = {%s}\n", who_am_i(), delta_fname[i], delta_file_timestamp);
			if( strcmp(delta_file_timestamp, full_file_timestamp) > 0 && strcmp(delta_file_timestamp, recovery_timestamp) <= 0 )
			{
				// rprintf(FWARNING, "[yee-%s] sender.c: make_delta_to_full delta_fname[%d]: %s is in the range, timestamp: %s\n", who_am_i(), i, delta_fname[i], delta_file_timestamp);
				delta_name_ptrs[delta_count_in_range] = delta_fname[i];
				delta_path_ptrs[delta_count_in_range] = delta_fpath[i];
				delta_count_in_range++;

			}
			// else
			// {
			// 	rprintf(FWARNING, "[yee-%s] sender.c: make_delta_to_full delta_fname[%d]: %s is not in the range\n", who_am_i(), i, delta_fname[i]);
			// }
		}

		rprintf(FWARNING, "[yee-%s] sender.c: make_delta_to_full delta_count_in_range = %d\n", who_am_i(), delta_count_in_range);
		qsort(delta_name_ptrs, delta_count_in_range, sizeof(const char*), compare_delta_file_name);
		qsort(delta_path_ptrs, delta_count_in_range, sizeof(const char*), compare_delta_file_name);

		for(int i = 0; i < delta_count_in_range; i++)
		{
			rprintf(FWARNING, "[yee-%s] sneder.c: make_d2f delta_name_ptrs[%d]: %s\n", who_am_i(), i, delta_name_ptrs[i]);
		}

		char recovery_fname[MAXPATHLEN], tmp_file_name[MAXPATHLEN];
		char recovery_fpath[MAXPATHLEN], tmp_file_path[MAXPATHLEN];
		
		strcpy(recovery_fname, file_name);
		strcat(recovery_fname, ".recovery.tmp1");
		
		strcpy(recovery_fpath, dir_name);
		strcat(recovery_fpath, "/");
		strcat(recovery_fpath, file_name);
		strcat(recovery_fpath, ".backup/");
		strcat(recovery_fpath, recovery_fname);
		
		strcpy(tmp_file_name, file_name);
		strcat(tmp_file_name, ".recovery.tmp2");
		
		strcpy(tmp_file_path, dir_name);
		strcat(tmp_file_path, "/");
		strcat(tmp_file_path, file_name);
		strcat(tmp_file_path, ".backup/");
		strcat(tmp_file_path, tmp_file_name);

		FILE *full_file = fopen(full_fpath, "rb");	// 打开全量文件 拼接的基准文件
		FILE *delta_file = NULL;	// 打开增量文件 记录了所拼接的信息
		FILE *recovery_file = fopen(recovery_fpath, "wb");	// 打开恢复文件 也用作拼接临时写入文件

		

		for(int i = 0; i < delta_count_in_range; i++ ) // 需要做delta_count_in_range次拼接
		{
			rprintf(FWARNING, "[yee-%s] sender.c: make_d2f start make delta_fpath[%d]: %s\n", who_am_i(), i, delta_path_ptrs[i]);
			delta_file = fopen(delta_path_ptrs[i], "r");
			if( delta_file == NULL )
			{
				rprintf(FWARNING, "[yee-%s] sender.c: make_delta_to_full delta_file open failed\n", who_am_i());
				return -1;
			}

			char line[1024];		// delta行读取缓冲区
			int delta_block_length = -1, delta_block_count = -1, delta_remainder_block_length = -1; // delta文件元数据 块大小 块数量 剩余块大小
			OFF_T total_size = -1, content_size = -1;	// delta文件元数据 偏移量 总大小 内容大小
			
			struct map_struct *mapbuf = NULL;	// map_struct 快速找到全量文件中匹配的内容
			int fd_full = do_open(full_fpath, O_RDONLY, 0);	// 以文件描述符的方式打开全量文件

			// delta 文件元数据解析
			if(fgets(line, sizeof(line), delta_file) != NULL)	
			{
				sscanf(line, "[delta file metadata] file_size = %ld, content_size = %ld, block_size = %d, block_count = %d, remainder_block = %d\n", 
					&total_size, &content_size, &delta_block_length, &delta_block_count, &delta_remainder_block_length);
			}	
			else
			{
				rprintf(FWARNING, "[yee-%s] sender.c: make_d2f fgets delta metadata error\n", who_am_i());
			}

			// 构建map_struct 以供map_ptr使用 参见receiver->receive_data
			int32 read_size = MAX(delta_block_length*2, 16*1024);
			mapbuf = map_file(fd_full, content_size, read_size, delta_block_length);	// 构建map_struct 以供map_ptr使用

			// delta 增量信息解析
			while ( fgets(line, sizeof(line), delta_file) != NULL)
			{ 
				int token = -1;
				OFF_T offset = -1, offset2 = -1;

				rprintf(FWARNING, "[yee-%s] sender.c: make_d2f line: %s\n", who_am_i(), line);
				if(strcmp(line, "\n") == 0 || strcmp(line, "\r\n") == 0)
				{
					continue;
				}
				else if(strncmp(line, "match token\0", strlen("match token\0")) == 0)	// 解析delta文件中匹配的部分 token
				{
					char *map_data = NULL;
					size_t map_len = -1;
					sscanf(line, "match token = %d, offset = %ld, offset2 = %ld\n", &token, &offset, &offset2);
					
					map_len = delta_block_length;
					if( token == delta_block_count - 1 && delta_remainder_block_length != 0)
					{
						map_len = delta_remainder_block_length;
					}
					
					map_data = map_ptr(mapbuf, offset2, map_len);	// 使用offset2

					if( map_data != NULL && fwrite(map_data, 1, map_len, recovery_file) != map_len )
					{
						rprintf(FWARNING, "[yee-%s] sender.c: make_d2f fwrite match data error\n", who_am_i());
					}

					// rprintf(FWARNING, "[yee-%s] sender.c: make_d2f match token = %d, offset = %ld, offset2 = %ld\nmap_data = \n%s\n", 
					// 		who_am_i(), token, offset, offset2, map_data);
					rprintf(FWARNING, "[yee-%s] sender.c: make_d2f match token = %d, offset = %ld, offset2 = %ld\n", 
							who_am_i(), token, offset, offset2);
				}
				else if(strncmp(line, "unmatch data length\0", strlen("unmatch data length\0")) == 0) // 解析delta文件中不匹配的部分
				{

					char unmatch_data[1024*1000];	// 不匹配数据缓冲区
				  	size_t unmatch_len = 0;			// 读取长度
					sscanf(line, "unmatch data length = %ld, offset = %ld\n", &unmatch_len, &offset);

					if( fread(unmatch_data, 1, unmatch_len, delta_file) != unmatch_len )
					{
						rprintf(FWARNING, "[yee-%s] sender.c: make_d2f fread unmatch data length error\n", who_am_i());
					}

					// rprintf(FWARNING, "[yee-%s] sender.c: make_d2f unmatch data length = %ld, offset = %ld\nunmatch_data = \n%s\n",
					// 		 who_am_i(), unmatch_len, offset, unmatch_data);
					rprintf(FWARNING, "[yee-%s] sender.c: make_d2f unmatch data length = %ld, offset = %ld\n",
							 who_am_i(), unmatch_len, offset);


					if( fwrite(unmatch_data, 1, unmatch_len, recovery_file) != unmatch_len ) // 直接将不匹配数据写入
					{
						rprintf(FWARNING, "[yee-%s] sender.c: make_d2f fwrite unmatch data length error\n", who_am_i());
					}
				}
				else
				{
					rprintf(FWARNING, "[yee-%s] sender.c: make_d2f line: %s is illegal\n", who_am_i(), line);
				}
			}
		
			fclose(delta_file);
			fclose(full_file);
			fclose(recovery_file);
			
			if(i != delta_count_in_range - 1)
			{
				delta_file = fopen(delta_path_ptrs[i+1], "rb");

				copy_file(recovery_fpath, tmp_file_path, -1, 0666);
				
				full_file = fopen(tmp_file_path, "rb");
				recovery_file = fopen(recovery_fpath, "wb");

				strcpy(full_fpath, tmp_file_path);
			}
			else
			{
				remove(tmp_file_path);
			}
		}

	} else {
		rprintf(FWARNING, "[yee-%s] sender.c: make_delta_to_full opendir() error\n", who_am_i());
	}
	return 0;
}

void send_files(int f_in, int f_out)    
{

		if (source_is_remote_or_local == -1)
		{
			rprintf(FWARNING, "[yee-%s] sender.c: send_files source_is_remote_or_local has not been set \n", who_am_i());
		}
		else
		{
			task_type_backup_or_recovery_sender = source_is_remote_or_local; 

			if(task_type_backup_or_recovery_sender == 0)
				rprintf(FWARNING,"[yee-%s] sender.c: send_files this is a *%s* task \n", who_am_i(), "backup");
			else
				rprintf(FWARNING,"[yee-%s] sender.c: send_files this is a *%s* task \n", who_am_i(), "recovery");
		}

		// char cwd[4096];

		// if (getcwd(cwd, sizeof(cwd)) != NULL) {
		// 	rprintf(FWARNING, "[yee-%s] sender.c: send_files current working directory: %s\n", who_am_i(), cwd);
		// } else {
		// 	rprintf(FWARNING, "[yee-%s] sender.c: send_files getcwd() error\n", who_am_i());
		// }

		rprintf(FWARNING, "[yee-%s] sender.c: send_files recorvery version : %s\n", who_am_i(), recovery_version);

		// The sender receives the old file checksum list sent by generate, calculates the matched and unmatched file data, and sends it to the receiver (write buffer)
		int fd = -1;
		struct sum_struct *s;
		struct map_struct *mbuf = NULL;
		STRUCT_STAT st;
		char fname[MAXPATHLEN], xname[MAXPATHLEN];
		const char *path, *slash;
		uchar fnamecmp_type;
		int iflags, xlen;
		struct file_struct *file;
		int phase = 0, max_phase = protocol_version >= 29 ? 2 : 1;
		int itemizing = am_server ? logfile_format_has_i : stdout_format_has_i;
		enum logcode log_code = log_before_transfer ? FLOG : FINFO;
		int f_xfer = write_batch < 0 ? batch_fd : f_out;
		int save_io_error = io_error;
		int ndx, j;

		if (DEBUG_GTE(SEND, 1))
			rprintf(FINFO, "send_files starting\n");

		while (1) {
			if (inc_recurse) {
				send_extra_file_list(f_out, MIN_FILECNT_LOOKAHEAD);
				extra_flist_sending_enabled = !flist_eof;
			}

			/* This call also sets cur_flist. */
			ndx = read_ndx_and_attrs(f_in, f_out, &iflags, &fnamecmp_type,
									 xname, &xlen);
			extra_flist_sending_enabled = False;

			// rprintf(FWARNING, "[yee-%s] sender.c: send_files ndx: %d\n", who_am_i(), ndx);

			if (ndx == NDX_DONE) {
				if (!am_server && INFO_GTE(PROGRESS, 2) && cur_flist) {
					set_current_file_index(NULL, 0);
					end_progress(0);
				}
				if (inc_recurse && first_flist) {
					file_old_total -= first_flist->used;
					flist_free(first_flist);
					if (first_flist) {
						if (first_flist == cur_flist)
							file_old_total = cur_flist->used;
						write_ndx(f_out, NDX_DONE);
						continue;
					}
				}
				if (++phase > max_phase)
					break;
				if (DEBUG_GTE(SEND, 1))
					rprintf(FINFO, "send_files phase=%d\n", phase);
				write_ndx(f_out, NDX_DONE);
				continue;
			}

			if (inc_recurse)
				send_extra_file_list(f_out, MIN_FILECNT_LOOKAHEAD);

			if (ndx - cur_flist->ndx_start >= 0)
				file = cur_flist->files[ndx - cur_flist->ndx_start];
			else
				file = dir_flist->files[cur_flist->parent_ndx];
			if (F_PATHNAME(file)) {
				path = F_PATHNAME(file);
				slash = "/";
			} else {
				path = slash = "";
			}
			if (!change_pathname(file, NULL, 0))
				continue;
			f_name(file, fname);

			// rprintf(FWARNING, "[yee-%s] sender.c: send_files fname(pre make d2f): %s\n", who_am_i(), fname);
			if(task_type_backup_or_recovery_sender == 1)
			{
				rprintf(FWARNING, "[yee-%s] sender.c: send_files make d2f version = %s\n", who_am_i(), recovery_version);
				make_delta_to_full(fname, recovery_version);
			}

			if (DEBUG_GTE(SEND, 1))
				rprintf(FINFO, "send_files(%d, %s%s%s)\n", ndx, path,slash,fname);

#ifdef SUPPORT_XATTRS
			if (preserve_xattrs && iflags & ITEM_REPORT_XATTR && do_xfers
				&& !(want_xattr_optim && BITS_SET(iflags, ITEM_XNAME_FOLLOWS|ITEM_LOCAL_CHANGE)))
				recv_xattr_request(file, f_in);
#endif

			if (!(iflags & ITEM_TRANSFER)) {
				maybe_log_item(file, iflags, itemizing, xname);
				write_ndx_and_attrs(f_out, ndx, iflags, fname, file,
									fnamecmp_type, xname, xlen);
				if (iflags & ITEM_IS_NEW) {
					stats.created_files++;
					if (S_ISREG(file->mode)) {
						/* Nothing further to count. */
					} else if (S_ISDIR(file->mode))
						stats.created_dirs++;
#ifdef SUPPORT_LINKS
					else if (S_ISLNK(file->mode))
						stats.created_symlinks++;
#endif
					else if (IS_DEVICE(file->mode))
						stats.created_devices++;
					else
						stats.created_specials++;
				}
				continue;
			}
			if (phase == 2) {
				rprintf(FERROR,
						"got transfer request in phase 2 [%s]\n",
						who_am_i());
				exit_cleanup(RERR_PROTOCOL);
			}

			if (file->flags & FLAG_FILE_SENT) {
				if (csum_length == SHORT_SUM_LENGTH) {
					/* For inplace: redo phase turns off the backup
                     * flag so that we do a regular inplace send. */
					make_backups = -make_backups;
					append_mode = -append_mode;
					csum_length = SUM_LENGTH;
				}
			} else {
				if (csum_length != SHORT_SUM_LENGTH) {
					make_backups = -make_backups;
					append_mode = -append_mode;
					csum_length = SHORT_SUM_LENGTH;
				}
				if (iflags & ITEM_IS_NEW)
					stats.created_files++;
			}

			updating_basis_file = inplace && (protocol_version >= 29
											  ? fnamecmp_type == FNAMECMP_FNAME : make_backups <= 0);

			if (!am_server && INFO_GTE(PROGRESS, 1))
				set_current_file_index(file, ndx);
			stats.xferred_files++;
			stats.total_transferred_size += F_LENGTH(file);

			remember_initial_stats();

			if (!do_xfers) { /* log the transfer */
				log_item(FCLIENT, file, iflags, NULL);
				write_ndx_and_attrs(f_out, ndx, iflags, fname, file,
									fnamecmp_type, xname, xlen);
				continue;
			}

			if (!(s = receive_sums(f_in))) {
				io_error |= IOERR_GENERAL;
				rprintf(FERROR_XFER, "receive_sums failed\n");
				exit_cleanup(RERR_PROTOCOL);
			}

			gettimeofday(&end, NULL);
			printf("finish receive_sums(second) time = %ld us\n", (unsigned long)(1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec));
			//finish = clock();
			printf("finish receive_sums(second) CPU clock time is %f seconds \n ", (double)(1000000 * (end.tv_sec - start.tv_sec)) / CLOCKS_PER_SEC );
			
			// rprintf(FWARNING, "[yee-%s] sender.c: send_files fname(pre open and send): %s\n", who_am_i(), fname);

			
			// fd = do_open(fname, O_RDONLY, 0);
			if(task_type_backup_or_recovery_sender == 1)
			{
				rprintf(FWARNING, "[yee-%s] sender.c: send_files recovery_files fname(before alter): %s\n", who_am_i(), fname);
				// strcat(fname, ".recovery.tmp1");
				char *ptr = strrchr(fname, '/');
				char dir_name[MAXPATHLEN];
				char file_name[MAXPATHLEN];

				if(ptr != NULL)
				{
					strncpy(dir_name, fname, ptr - fname);
					dir_name[ptr - fname] = '\0';
					strcpy(file_name, ptr + 1);
				}
				else
				{
					strcpy(dir_name, ".");
					strcpy(file_name, fname);
				}

				strcpy(fname, dir_name);
				strcat(fname, "/");
				strcat(fname, file_name);
				strcat(fname, ".backup/");
				strcat(fname, file_name);
				strcat(fname, ".recovery.tmp1");
				rprintf(FWARNING, "[yee-%s] sender.c: send_files recovery_files fname(after alter): %s\n", who_am_i(), fname);
			}
			fd = do_open(fname, O_RDONLY, 0);
			if (fd == -1) {
				if (errno == ENOENT) {
					enum logcode c = am_daemon
									 && protocol_version < 28 ? FERROR
															  : FWARNING;
					io_error |= IOERR_VANISHED;
					rprintf(c, "file has vanished: %s\n",
							full_fname(fname));
				} else {
					io_error |= IOERR_GENERAL;
					rsyserr(FERROR_XFER, errno,
							"send_files failed to open %s",
							full_fname(fname));
				}
				free_sums(s);
				if (protocol_version >= 30)
					send_msg_int(MSG_NO_SEND, ndx);
				continue;
			}

			/* map the local file */
			if (do_fstat(fd, &st) != 0) {
				io_error |= IOERR_GENERAL;
				rsyserr(FERROR_XFER, errno, "fstat failed");
				free_sums(s);
				close(fd);
				exit_cleanup(RERR_FILEIO);
			}

			if (st.st_size) {
				int32 read_size = MAX(s->blength * 3, MAX_MAP_SIZE);
				mbuf = map_file(fd, st.st_size, read_size, s->blength);
			} else
				mbuf = NULL;

			if (DEBUG_GTE(DELTASUM, 2)) {
				rprintf(FINFO, "send_files mapped %s%s%s of size %s\n",
						path,slash,fname, big_num(st.st_size));
			}

			write_ndx_and_attrs(f_out, ndx, iflags, fname, file,
								fnamecmp_type, xname, xlen);
			write_sum_head(f_xfer, s);

			if (DEBUG_GTE(DELTASUM, 2))
				rprintf(FINFO, "calling match_sums %s%s%s\n", path,slash,fname);

			if (log_before_transfer)
				log_item(FCLIENT, file, iflags, NULL);
			else if (!am_server && INFO_GTE(NAME, 1) && INFO_EQ(PROGRESS, 1))
				rprintf(FCLIENT, "%s\n", fname);

			set_compression(fname);

			match_sums(f_xfer, s, mbuf, st.st_size);

			if (INFO_GTE(PROGRESS, 1))
				end_progress(st.st_size);

			log_item(log_code, file, iflags, NULL);

			if (mbuf) {
				j = unmap_file(mbuf);
				if (j) {
					io_error |= IOERR_GENERAL;
					rsyserr(FERROR_XFER, j,
							"read errors mapping %s",
							full_fname(fname));
				}
			}
			close(fd);

			free_sums(s);

			if (DEBUG_GTE(SEND, 1))
				rprintf(FINFO, "sender finished %s%s%s\n", path,slash,fname);

			/* Flag that we actually sent this entry. */
			file->flags |= FLAG_FILE_SENT;
		}
		if (make_backups < 0)
			make_backups = -make_backups;

		if (io_error != save_io_error && protocol_version >= 30)
			send_msg_int(MSG_IO_ERROR, io_error);

		if (DEBUG_GTE(SEND, 1))
			rprintf(FINFO, "send files finished\n");

		match_report();

		write_ndx(f_out, NDX_DONE);
}
