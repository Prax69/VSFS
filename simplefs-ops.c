#include "simplefs-ops.h"
extern struct filehandle_t file_handle_array[MAX_OPEN_FILES]; // Array for storing opened files

int simplefs_create(char *filename){
    /*
	    Create file with name `filename` from disk
	*/
		
	int file_idx = simplefs_allocInode();
	if(file_idx == -1) return -1;
	
	struct inode_t *inode = (struct inode_t *)malloc(sizeof(struct inode_t)); // present in disk-emulation
	
	simplefs_readInode (file_idx, inode); 				// reading INODE from disk
	inode -> status = INODE_IN_USE; 					// settign INODE in use
	for(int i=0; i<sizeof(filename);i++){
	inode -> name[i] = filename[i];
	}									// copying filename
	inode->file_size=0;
	simplefs_writeInode(file_idx,inode); 					// now updating the INODE in disk
	free(inode); 								// clean up
	return file_idx;
	
    //return 0;
}


void simplefs_delete(char *filename){
    /*
	    delete file with name `filename` from disk
	*/
	
	struct inode_t *inode = (struct inode_t*)malloc(sizeof(struct inode_t)); // allocating inode
	
	for(int i=0;i<NUM_INODES;i++){
	simplefs_readInode(i,inode); // read from disk
									// check if in use already and for filename
	if(inode->status == INODE_IN_USE && !strcmp(inode->name,filename)){
									// free data blocks corresponding to the file to delete
		for(int i=0;i<MAX_FILE_SIZE;i++){
			if(inode->direct_blocks[i] != -1){
			simplefs_freeDataBlock(inode->direct_blocks[i]);
			}
		}
									// freeing the inode
		simplefs_freeInode(i);
									// cleanup
		free(inode);
		return;
		
	}
	}
	
}

int simplefs_open(char *filename){
    /*
	    open file with name `filename`
	*/
	struct inode_t *inode=(struct inode_t*)malloc(sizeof(struct inode_t));
		for(int i=0;i<NUM_INODES;i++){
		simplefs_readInode(i,inode); 						// finding the file
		if(inode->status == INODE_IN_USE && !strcmp(inode->name,filename)){
			for(int j=0;j<MAX_OPEN_FILES;j++){
				if(file_handle_array[j].inode_number == -1){ 		// to find an available slot
					file_handle_array[j].inode_number = i;
					file_handle_array[j].offset =0;
					return j; 					// returning the file handle
				}
			}
		}
	}
    return 0;
}

void simplefs_close(int file_handle){
    /*
	    close file pointed by `file_handle`
	*/
	file_handle_array[file_handle].inode_number = -1;			// reseting properties corresponding to the file
	file_handle_array[file_handle].offset=0;

}

int simplefs_read(int file_handle, char *buf, int nbytes){
    /*
	    read `nbytes` of data into `buf` from file pointed by `file_handle` starting at current offset
	*/
	
	struct filehandle_t file = file_handle_array[file_handle]; 	// getting file corresponding to the given file handler
	if (file.offset + nbytes > MAX_FILE_SIZE * BLOCKSIZE)  	//edge case where reading is exceeding the file size
		return 0;

	struct inode_t *inode = (struct inode_t *)malloc(sizeof(struct inode_t));
	simplefs_readInode(file.inode_number, inode);

	char temp[BLOCKSIZE];
	int bytesRead = 0;
	int off = file.offset;

	for (int j = 0; j < MAX_FILE_SIZE; j++){  			// Reading data from direct blocks
		if(off < (j + 1) * BLOCKSIZE) {
			if (inode->direct_blocks[j] != -1 && bytesRead < nbytes){ 			// reading data to temp buffer 
				simplefs_readDataBlock(inode->direct_blocks[j], temp);

				for(int i = 0; i < BLOCKSIZE; i++) {
					if(i >= off - j * BLOCKSIZE && bytesRead < nbytes) {
						buf[bytesRead] = temp[i];
						bytesRead++;
					}
				}	
				off = (j + 1)*BLOCKSIZE;
				if(bytesRead == nbytes) break;
			}
		}
	}	

	free(inode); 	//cleanup
    return 0;
}


int simplefs_write(int file_handle, char *buf, int nbytes){
    /*
	    write `nbytes` of data from `buf` to file pointed by `file_handle` starting at current offset
	*/
	struct filehandle_t file = file_handle_array[file_handle];    // getting file corresponding to given file_handle
	if(file.offset + nbytes > MAX_FILE_SIZE * BLOCKSIZE) return -1;
	
	struct inode_t *inode=(struct inode_t*)malloc(sizeof(struct inode_t));
	simplefs_readInode(file.inode_number,inode);
	
	inode->file_size +=nbytes;	// updating file size 
	char temp[BLOCKSIZE];
	int bytesWritten = 0;
	int oSet = file.offset;
	
	for(int j=0;j<MAX_FILE_SIZE;j++){	
		if(oSet < (j+1)*BLOCKSIZE){
		if(inode->direct_blocks[j] != -1){
			simplefs_readDataBlock(inode->direct_blocks[j],temp);
			for(int k=0;k<BLOCKSIZE;k++){
				if( k >=oSet - j*BLOCKSIZE && bytesWritten < nbytes){
				temp[k] = buf[bytesWritten];
				bytesWritten++;
				}
			}
		oSet = (j+1)*BLOCKSIZE;
										// writing updated data block back to the disk
		simplefs_writeDataBlock(inode->direct_blocks[j],temp);
		simplefs_writeInode(file.inode_number,inode);			
		}
		
		else if(inode->direct_blocks[j] == -1){
			for(int k=0;k< BLOCKSIZE;k++){
			if(bytesWritten < nbytes){
				temp[k] = buf[bytesWritten];
				bytesWritten++;
				}
			}
			oSet = (j+1)*BLOCKSIZE;
			
			int blocknum = simplefs_allocDataBlock(); // allocating a new data block
			if(blocknum == -1)return -1;
								// writing a new data block to disk
			inode->direct_blocks[j] = blocknum;
			simplefs_writeDataBlock(inode->direct_blocks[j],temp);
			simplefs_writeInode(file.inode_number,inode);
		}
		
		if(bytesWritten == nbytes)break;
		}
	}
	
	free(inode);	
    return 0;
}


int simplefs_seek(int file_handle, int nseek){
    /*
	   increase `file_handle` offset by `nseek`
	*/
	int oSet = file_handle_array[file_handle].offset;
	file_handle_array[file_handle].offset+=nseek;
	
	// if new offset is out of valid bounds rollback to original offset
	
	if(file_handle_array[file_handle].offset  > MAX_FILE_SIZE * BLOCKSIZE || file_handle_array[file_handle].offset <0){
	file_handle_array[file_handle].offset = oSet;
	return -1; // unsuccessful seek
	}	
	
    return 0; // successful seek
}
