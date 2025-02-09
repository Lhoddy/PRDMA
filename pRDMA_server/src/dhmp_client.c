#include "dhmp.h"
#include "dhmp_log.h"
#include "dhmp_hash.h"
#include "dhmp_config.h"
#include "dhmp_context.h"
#include "dhmp_dev.h"
#include "dhmp_transport.h"
#include "dhmp_task.h"
#include "dhmp_work.h"
#include "dhmp_client.h"
struct dhmp_client *client=NULL;

struct timespec time_start, time_end;
unsigned long time_diff_ns;

static void show(const char * str, struct timespec* time, char output)
{
	fprintf(stderr,str);	
	fflush(stderr);
	clock_gettime(CLOCK_MONOTONIC, time);	
	if(output == 1)
	{
		time_diff_ns = ((time_end.tv_sec * 1000000000) + time_end.tv_nsec) -
                        ((time_start.tv_sec * 1000000000) + time_start.tv_nsec);
  		fprintf(stderr,"runtime %lf ms\n", (double)time_diff_ns/1000000);
  		fflush(stderr);
	}
}

struct dhmp_transport* dhmp_get_trans_from_addr(void *dhmp_addr)
{
	return client->connect_trans;
}

void *dhmp_malloc(size_t length, int is_special)
{
	struct dhmp_malloc_work malloc_work;
	struct dhmp_work *work;
	struct dhmp_addr_info *addr_info;
	
	if(length<0)
	{
		ERROR_LOG("length is error.");
		goto out;
	}

	/*select which node to alloc nvm memory*/
	if(!client->connect_trans)
	{
		ERROR_LOG("don't exist remote server.");
		goto out;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("allocate memory error.");
		goto out;
	}
	
	addr_info=malloc(sizeof(struct dhmp_addr_info));
	if(!addr_info)
	{
		ERROR_LOG("allocate memory error.");
		goto out_work;
	}
	addr_info->nvm_mr.length=0;
	
	malloc_work.addr_info=addr_info;
	malloc_work.rdma_trans= client->connect_trans;
	malloc_work.length=length;
	malloc_work.done_flag=false;
	malloc_work.is_special = is_special;
	malloc_work.recv_flag=false;

	work->work_type=DHMP_WORK_MALLOC;
	work->work_data=&malloc_work;

	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!malloc_work.done_flag);

	free(work);
	
	return malloc_work.res_addr;

out_work:
	free(work);
out:
	return NULL;
}



void dhmp_free(void *dhmp_addr)
{
	struct dhmp_free_work free_work;
	struct dhmp_work *work;
	struct dhmp_transport *rdma_trans;
	
	if(dhmp_addr==NULL)
	{
		ERROR_LOG("dhmp address is NULL");
		return ;
	}

	rdma_trans=dhmp_get_trans_from_addr(dhmp_addr);
	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return ;
	}
	
	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("allocate memory error.");
		return ;
	}

	free_work.rdma_trans=rdma_trans;
	free_work.dhmp_addr=dhmp_addr;
	free_work.done_flag=false;
	
	work->work_type=DHMP_WORK_FREE;
	work->work_data=&free_work;

	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!free_work.done_flag);

	free(work);
}

int dhmp_read(void *dhmp_addr, void * local_buf, size_t count)
{
	struct dhmp_transport *rdma_trans=NULL;
	struct dhmp_rw_work rwork;
	struct dhmp_work *work,*con;
	
	rdma_trans=dhmp_get_trans_from_addr(dhmp_addr);;
	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return -1;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("allocate memory error.");
		return -1;
	}
	
	rwork.done_flag=false;
	rwork.length=count;
	rwork.local_addr=local_buf;
	rwork.dhmp_addr=dhmp_addr;
	rwork.rdma_trans=rdma_trans;
	
	work->work_type=DHMP_WORK_READ;
	work->work_data=&rwork;
	
	con = work;
	//INFO_LOG("work = %p",work);	
	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);

	while(!rwork.done_flag);
	free(con);

	return 0;
}

int dhmp_write(void *dhmp_addr, void * local_buf, size_t count)
{
	struct dhmp_transport *rdma_trans=NULL;
	struct dhmp_rw_work wwork;
	struct dhmp_work *work;

	rdma_trans=dhmp_get_trans_from_addr(dhmp_addr);
	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return -1;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("alloc memory error.");
		return -1;
	}
	wwork.done_flag=false;
	wwork.length=count;
	wwork.local_addr=local_buf;
	wwork.dhmp_addr=dhmp_addr;
	wwork.rdma_trans=rdma_trans;
			
	work->work_type=DHMP_WORK_WRITE;
	work->work_data=&wwork;
	
	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!wwork.done_flag);

	free(work);
	
	return 0;
}


int amper_clover_compare_and_set(void *dhmp_addr, size_t length, uint64_t value)
{
	struct dhmp_transport *rdma_trans=NULL;
	struct amper_clover_work wwork;
	struct dhmp_work *work;

	rdma_trans=dhmp_get_trans_from_addr(dhmp_addr);
	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return -1;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("alloc memory error.");
		return -1;
	}
	wwork.done_flag=false;
	wwork.dhmp_addr=dhmp_addr;
	wwork.rdma_trans=rdma_trans;
	wwork.length = length;
	wwork.value =value;
			
	work->work_type=AMPER_WORK_CLOVER;
	work->work_data=&wwork;
	
	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!wwork.done_flag);

	free(work);
	
	return 0;
}

int amper_write_L5( void * local_buf, size_t count, void * globle_addr, char flag_write)
{
	struct dhmp_transport *rdma_trans= client->connect_trans; // assume only one server
	struct amper_L5_work wwork;
	struct dhmp_work *work;

	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return -1;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("alloc memory error.");
		return -1;
	}
	wwork.done_flag=false;
	wwork.length=count;
	wwork.local_addr=local_buf;
	wwork.rdma_trans=rdma_trans;
	wwork.dhmp_addr = globle_addr;
	wwork.flag_write = flag_write;
			
	work->work_type=AMPER_WORK_L5;
	work->work_data=&wwork;
	
	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!wwork.done_flag);

	free(work);
	
	return 0;
}

void check_request(int model)
{
	int i;
	for(i = 0; i< FaRM_buffer_NUM;i++)
	{
		pthread_mutex_lock(&client->mutex_request_num);
		if(model == 1)
			while(client->FaRM.is_available[i] == 0)
			{
				pthread_mutex_unlock(&client->mutex_request_num);
				pthread_mutex_lock(&client->mutex_request_num);
			}
		else if(model == 2)
		{
			while(client->pRDMA.is_available[i].value == 0)
			{
				pthread_mutex_unlock(&client->mutex_request_num);
				pthread_mutex_lock(&client->mutex_request_num);
			}
		}
		else if(model == 3)
		{
			while(client->para_request_num != 5)
			{
				pthread_mutex_unlock(&client->mutex_request_num);
				pthread_mutex_lock(&client->mutex_request_num);
			}
		}
		pthread_mutex_unlock(&client->mutex_request_num);
	}
}



void amper_pRDMA_write_senderactive( void * local_buf, size_t count, void * globle_addr, char flag_write)
{
	struct dhmp_transport *rdma_trans= client->connect_trans; // assume only one server
	struct amper_L5_work wwork; //L5 = prdma write wflush work
	struct dhmp_work *work;

	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return ;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("alloc memory error.");
		return ;
	}
	wwork.done_flag=false;
	wwork.length=count;
	wwork.local_addr=local_buf;
	wwork.rdma_trans=rdma_trans;
	wwork.dhmp_addr = globle_addr;
	wwork.flag_write = flag_write;
			
	work->work_type=AMPER_WORK_pRDMA_WS;
	work->work_data=&wwork;
	
	while(1)
	{
		pthread_mutex_lock(&client->mutex_request_num);
		if(client->pRDMA.is_available[client->pRDMA.Scur].value == 0)
		{
			pthread_mutex_unlock(&client->mutex_request_num);
			continue;
		}
		client->pRDMA.is_available[client->pRDMA.Scur].value  = 0;
		client->pRDMA.is_available[client->pRDMA.Scur].work = work;
		wwork.cur = client->pRDMA.Scur;
		client->pRDMA.Scur= (client->pRDMA.Scur + 1) % FaRM_buffer_NUM;
		pthread_mutex_unlock(&client->mutex_request_num);
		break;
	}
	
	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!wwork.done_flag);
	
	return ;
}

void model_FaRM( void * local_buf, size_t count, void * globle_addr, char flag_write)
{
	struct dhmp_transport *rdma_trans= client->connect_trans; // assume only one server
	struct amper_L5_work wwork; //L5 = Farm work
	struct dhmp_work *work;

	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return ;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("alloc memory error.");
		return ;
	}
	wwork.done_flag=false;
	wwork.length=count;
	wwork.local_addr=local_buf;
	wwork.rdma_trans=rdma_trans;
	wwork.dhmp_addr = globle_addr;
	wwork.flag_write = flag_write;
			
	work->work_type=AMPER_WORK_FaRM;
	work->work_data=&wwork;
	
	while(1)
	{
		pthread_mutex_lock(&client->mutex_request_num);
		if(client->FaRM.is_available[client->FaRM.Scur] == 0)
		{
			pthread_mutex_unlock(&client->mutex_request_num);
			continue;
		}
		client->FaRM.is_available[client->FaRM.Scur] = 0;
		wwork.cur = client->FaRM.Scur;
		client->FaRM.Scur= (client->FaRM.Scur + 1) % FaRM_buffer_NUM;
		pthread_mutex_unlock(&client->mutex_request_num);
		break;
	}
	

	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!wwork.done_flag);

	free(work);
	
	return ;
}



void model_herd( void * local_buf, size_t count, void * globle_addr, char flag_write)
{
	struct dhmp_transport *rdma_trans= client->connect_trans; // assume only one server
	struct amper_L5_work wwork; //L5 = Farm work = herd
	struct dhmp_work *work;

	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return ;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("alloc memory error.");
		return ;
	}
	wwork.done_flag=false;
	wwork.length=count;
	wwork.local_addr=local_buf;
	wwork.rdma_trans=rdma_trans;
	wwork.dhmp_addr = globle_addr;
	wwork.flag_write = flag_write;
			
	work->work_type=AMPER_WORK_Herd;
	work->work_data=&wwork;
	
	while(1)
	{
		pthread_mutex_lock(&client->mutex_request_num);
		if(client->herd.is_available[client->herd.Scur] == 0)
		{
			pthread_mutex_unlock(&client->mutex_request_num);
			continue;
		}
		client->herd.is_available[client->herd.Scur] = 0;
		wwork.cur = client->herd.Scur;
		client->herd.Scur= (client->herd.Scur + 1) % FaRM_buffer_NUM;
		pthread_mutex_unlock(&client->mutex_request_num);
		break;
	}

	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!wwork.done_flag);

	free(work);
	
	return ;
}

int amper_scalable(size_t offset, size_t count, int write_type, size_t length, char flag_write, char batch)
{
	struct dhmp_transport *rdma_trans= client->connect_trans; // assume only one server
	struct amper_scalable_work wwork;
	struct dhmp_work *work;

	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return -1;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("alloc memory error.");
		return -1;
	}
	wwork.done_flag=false;
	wwork.length=count;
	wwork.size = length;
	wwork.write_type = write_type;
	wwork.rdma_trans=rdma_trans;
	wwork.flag_write = flag_write;
	wwork.batch = batch;
	wwork.offset = offset;
			
	work->work_type=AMPER_WORK_scalable;
	work->work_data=&wwork;
	
	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!wwork.done_flag);

	free(work);
	
	return 0;
}

int amper_sendRPC_Tailwind(size_t offset, struct ibv_mr*head_mr, size_t head_size , void * local_buf, size_t size)
{
	struct dhmp_transport *rdma_trans= client->connect_trans; // assume only one server
	struct amper_Tailwind_work wwork;
	struct dhmp_work *work;

	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return -1;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("alloc memory error.");
		return -1;
	}
	wwork.done_flag=false;
	wwork.recv_flag = false;
	wwork.length=size;
	wwork.offset = offset;
	wwork.local_addr=local_buf;
	wwork.rdma_trans=rdma_trans;
	wwork.head_size=head_size;
	wwork.head_mr=head_mr;
			
	work->work_type=AMPER_WORK_Tailwind_RPC;
	work->work_data=&wwork;
	
	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!wwork.done_flag);

	free(work);
	
	return 0;
}




int amper_write_Tailwind(size_t offset, struct ibv_mr*head_mr, size_t head_size , void * local_buf, size_t size)
{
	struct dhmp_transport *rdma_trans= client->connect_trans; // assume only one server
	struct amper_Tailwind_work wwork;
	struct dhmp_work *work;

	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return -1;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("alloc memory error.");
		return -1;
	}
	wwork.done_flag=false;
	wwork.length=size;
	wwork.offset = offset;
	wwork.local_addr=local_buf;
	wwork.rdma_trans=rdma_trans;
	wwork.head_size=head_size;
	wwork.head_mr=head_mr;
			
	work->work_type=AMPER_WORK_Tailwind;
	work->work_data=&wwork;
	
	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!wwork.done_flag);

	free(work);
	
	return 0;
}


int amper_sendRPC_DaRPC( uintptr_t * globle_addr , size_t length, uintptr_t * local_addr, char flag_write, char batch)
{
	struct dhmp_transport *rdma_trans= client->connect_trans; // assume only one server
	struct amper_DaRPC_work wwork;
	struct dhmp_work *work;

	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return -1;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("alloc memory error.");
		return -1;
	}
	wwork.done_flag=false;
	wwork.recv_flag = false;
	wwork.length= length;
	wwork.local_addr=local_addr;
	wwork.rdma_trans=rdma_trans;
	wwork.globle_addr = globle_addr;
	wwork.flag_write = flag_write;
	wwork.batch = batch;
			
	work->work_type=AMPER_WORK_DaRPC;
	work->work_data=&wwork;
	
	while(1)
	{
		pthread_mutex_lock(&client->mutex_request_num);
		if(client->para_request_num == 0)
		{
			pthread_mutex_unlock(&client->mutex_request_num);
			continue;
		}
		client->para_request_num --;
		pthread_mutex_unlock(&client->mutex_request_num);
		break;
	}

	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!wwork.done_flag);

	free(work);
	
	return 0;
}


int amper_RFP(void * local_buf, uintptr_t globle_addr, size_t count, bool is_write)
{
	struct dhmp_transport *rdma_trans= client->connect_trans;
	struct dhmp_RFP_work wwork;
	struct dhmp_work *work;
	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return -1;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("alloc memory error.");
		return -1;
	}
	wwork.done_flag=false;
	wwork.length=count;
	wwork.local_addr=local_buf;
	wwork.rdma_trans=rdma_trans;
	wwork.is_write = is_write;
	wwork.dhmp_addr = globle_addr;
			
	work->work_type=AMPER_WORK_RFP;
	work->work_data=&wwork;
	
	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!wwork.done_flag);

	free(work);
	
	return 0;
}


int dhmp_write2(void *dhmp_addr, void * local_buf, size_t count)
{
	struct dhmp_transport *rdma_trans=NULL;
	struct dhmp_rw2_work wwork;
	struct dhmp_work *work;
	rdma_trans=dhmp_get_trans_from_addr(dhmp_addr);
	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return -1;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("alloc memory error.");
		return -1;
	}
	wwork.done_flag=false;
	wwork.length=count;
	wwork.local_addr=local_buf;
	wwork.dhmp_addr=dhmp_addr;
	wwork.rdma_trans=rdma_trans;
	wwork.S_ringMR = &(client->ringbuffer.mr);	
		
	work->work_type=DHMP_WORK_WRITE2;
	work->work_data=&wwork;
	
	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!wwork.done_flag);

	free(work);
	
	return 0;
}

int dhmp_send(void *dhmp_addr, void * local_buf, size_t count, bool is_write)
{
	struct dhmp_transport *rdma_trans=NULL;
	struct dhmp_Send_work wwork;
	struct dhmp_work *work;

	rdma_trans=dhmp_get_trans_from_addr(dhmp_addr);
	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return -1;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("alloc memory error.");
		return -1;
	}
	wwork.done_flag=false;
	wwork.recv_flag = false;
	wwork.length=count;
	wwork.rdma_trans=rdma_trans;
	wwork.local_addr = local_buf;
	wwork.is_write = is_write;
	wwork.dhmp_addr = dhmp_addr;
		
	work->work_type=DHMP_WORK_SEND;
	work->work_data=&wwork;
	
	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	while(!wwork.done_flag);
	free(work);
	return 0;
}


void *GetAddr_request1(void * dhmp_addr, size_t length, uint32_t * task_offset,  bool * cmpflag)
{
	struct dhmp_transport *rdma_trans=NULL;
	struct reqAddr_work wwork;    
	struct dhmp_work *work;

	rdma_trans=dhmp_get_trans_from_addr(dhmp_addr);
	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return NULL;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("alloc memory error.");
		return NULL;
	}
	wwork.done_flag=false;
	wwork.recv_flag = false;
	wwork.length=length;
	wwork.rdma_trans=rdma_trans;
	wwork.dhmp_addr = dhmp_addr;	
	wwork.cmpflag = cmpflag;  		

	work->work_type=DHMP_WORK_REQADDR1;  
	work->work_data=&wwork;
	
	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!wwork.done_flag);

	free(work);
	if(task_offset != NULL)	
		*task_offset = (uint32_t)wwork.length;
	return wwork.dhmp_addr;
}

int dhmp_write_imm(void *dhmp_addr, void * local_buf, size_t length, uint32_t task_offset ,bool *cmpflag)
{
	struct dhmp_transport *rdma_trans=NULL;
	struct dhmp_writeImm_work wwork;
	struct dhmp_work *work;

	rdma_trans=dhmp_get_trans_from_addr(dhmp_addr);
	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return -1;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("alloc memory error.");
		return -1;
	}
	wwork.done_flag=false;
	wwork.recv_flag = false;
	wwork.length=length;
	wwork.rdma_trans=rdma_trans;
	wwork.local_addr = local_buf;
	wwork.dhmp_addr = dhmp_addr;	
	wwork.task_offset = task_offset;
		
	work->work_type=DHMP_WORK_WRITEIMM;
	work->work_data=&wwork;
	
	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!wwork.done_flag);

	free(work);
	
	return 0;
}

int dhmp_write_imm2(void * server_addr,void *dhmp_addr, void * local_buf, size_t length)
{
	struct dhmp_transport *rdma_trans=NULL;
	struct dhmp_writeImm2_work wwork;
	struct dhmp_work *work;

	rdma_trans=dhmp_get_trans_from_addr(dhmp_addr);
	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return -1;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("alloc memory error.");
		return -1;
	}
	wwork.done_flag=false;
	wwork.recv_flag = false;
	wwork.length=length;
	wwork.rdma_trans=rdma_trans;
	wwork.local_addr = local_buf;
	wwork.dhmp_addr = dhmp_addr;	
	wwork.server_addr = server_addr;
	wwork.S_ringMR = &(client->ringbuffer.mr);
			
	work->work_type=DHMP_WORK_WRITEIMM2;
	work->work_data=&wwork;
	
	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!wwork.done_flag);

	free(work);
	
	return 0;
}

int dhmp_write_imm3(void *dhmp_addr, void * local_buf, size_t length)
{

	struct dhmp_transport *rdma_trans=NULL;
	struct dhmp_writeImm3_work wwork;
	struct dhmp_work *work;

	rdma_trans=dhmp_get_trans_from_addr(dhmp_addr);
	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return -1;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("alloc memory error.");
		return -1;
	}
	wwork.done_flag=false;
	wwork.recv_flag=false;
	wwork.length=length;
	wwork.rdma_trans=rdma_trans;
	wwork.local_addr = local_buf;
	wwork.dhmp_addr = dhmp_addr;	
	wwork.S_ringMR = &(client->ringbuffer.mr);
		
	work->work_type=DHMP_WORK_WRITEIMM3;
	work->work_data=&wwork;

	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
//	dhmp_WriteImm3_work_handler(work);
	while(!wwork.done_flag);
	free(work);
	return 0;
}

int dhmp_send2(void *server_addr, void *dhmp_addr, void * local_buf, size_t length)
{
	struct dhmp_transport *rdma_trans=NULL;
	struct dhmp_Send2_work wwork;
	struct dhmp_work *work;

	rdma_trans=dhmp_get_trans_from_addr(dhmp_addr);
	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return -1;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("alloc memory error.");
		return -1;
	}
	wwork.done_flag=false;
	wwork.recv_flag = false;
	wwork.length=length;
	wwork.rdma_trans=rdma_trans;
	wwork.local_addr = local_buf;
	wwork.dhmp_addr = dhmp_addr;	
	wwork.server_addr = server_addr;
		
	work->work_type=DHMP_WORK_SEND2;
	work->work_data=&wwork;
	
	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!wwork.done_flag);

	free(work);
	
	return 0;
}


int dhmp_Sread(void *dhmp_addr, void * local_buf, size_t length)
{
	INFO_LOG("dhmp_Sread");
	struct dhmp_transport *rdma_trans=NULL;
	struct dhmp_Sread_work wwork;
	struct dhmp_work *work;

	rdma_trans=dhmp_get_trans_from_addr(dhmp_addr);
	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return -1;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("alloc memory error.");
		return -1;
	}
	wwork.done_flag=false;
	wwork.recv_flag = false;
	wwork.length=length;
	wwork.rdma_trans=rdma_trans;
	wwork.local_addr = local_buf;
	wwork.dhmp_addr = dhmp_addr;	
		
	work->work_type=DHMP_WORK_SREAD;
	work->work_data=&wwork;
	
	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!wwork.done_flag);

	free(work);
	
	return 0;
}



struct dhmp_device *dhmp_get_dev_from_client()
{
	struct dhmp_device *res_dev_ptr=NULL;
	if(!list_empty(&client->dev_list))
	{
		res_dev_ptr=list_first_entry(&client->dev_list,
									struct dhmp_device,
									dev_entry);
	}
		
	return res_dev_ptr;
}


void dhmp_client_init(size_t size,int obj_num)
{
	int i;
	void *temp;
	size_t buffer_size;
	
	client=(struct dhmp_client *)malloc(sizeof(struct dhmp_client));
	if(!client)
	{
		ERROR_LOG("alloc memory error.");
		return ;
	}

	dhmp_hash_init();
	dhmp_config_init(&client->config, true);
	dhmp_context_init(&client->ctx);
	
	/*init list about rdma device*/
	INIT_LIST_HEAD(&client->dev_list);
	dhmp_dev_list_init(&client->dev_list);


	/*init the addr hash table of client*/
	for(i=0;i<DHMP_CLIENT_HT_SIZE;i++)
	{
		INIT_HLIST_HEAD(&client->addr_info_ht[i]);
	}

	
	/*init the structure about send mr list */
	pthread_mutex_init(&client->mutex_send_mr_list, NULL);
	INIT_LIST_HEAD(&client->send_mr_list);

	
	/*init normal connection*/
	for(i=0;i<client->config.nets_cnt;i++)
	{
		INFO_LOG("create the [%d]-th normal transport.",i);
		client->connect_trans=dhmp_transport_create(&client->ctx, 
														dhmp_get_dev_from_client(),
														false,
														false);
		if(!client->connect_trans)
		{
			ERROR_LOG("create the [%d]-th transport error.",i);
			continue;
		}
		client->connect_trans->node_id=i;
#ifdef UD
		dhmp_transport_connect_UD(client->connect_trans,
							client->config.net_infos[i].addr,
							client->config.net_infos[i].port);
#else
		dhmp_transport_connect(client->connect_trans,
							client->config.net_infos[i].addr,
							client->config.net_infos[i].port);
#endif
	}

	for(i=0;i<client->config.nets_cnt;i++)
	{
		if(i == 1) ERROR_LOG("more than one server or client");
		if(client->connect_trans==NULL)
			continue;
		while(client->connect_trans->trans_state<DHMP_TRANSPORT_STATE_CONNECTED);
		temp = malloc(8);
		client->cas_mr = dhmp_create_smr_per_ops(client->connect_trans, temp, 8);

		client->per_ops_mr_addr = malloc(size+1024);//request+data
		memset(client->per_ops_mr_addr, 0 ,size+1024);
		client->per_ops_mr =dhmp_create_smr_per_ops(client->connect_trans, client->per_ops_mr_addr, size+1024);
		client->per_ops_mr_addr2 = malloc(size+1024);//request+data
		memset(client->per_ops_mr_addr2, 0 ,size+1024);
		client->per_ops_mr2 =dhmp_create_smr_per_ops(client->connect_trans, client->per_ops_mr_addr2, size+1024);
	}


	
	/*init the structure about work thread*/
	pthread_mutex_init(&client->mutex_work_list, NULL);
	pthread_mutex_init(&client->mutex_request_num, NULL);
	INIT_LIST_HEAD(&client->work_list);
	pthread_create(&client->work_thread, NULL, dhmp_work_handle_thread, (void*)client);

	// dhmp_malloc(0,1);// ringbuffer
	{//scaleRPC
		client->scaleRPC.Creq_mr = client->per_ops_mr->mr;
		client->scaleRPC.Cdata_mr = client->per_ops_mr2->mr;
	}
	{//L5
		client->local_mr = client->per_ops_mr->mr;
		buffer_size = sizeof(uintptr_t) + sizeof(size_t)+1 + size;
		temp = malloc(buffer_size);
		client->L5.local_smr1 = dhmp_create_smr_per_ops(client->connect_trans, temp, buffer_size);
	}
	{//RFP
		buffer_size = sizeof(uintptr_t) + sizeof(size_t)+2+ size;
		temp = malloc(buffer_size);
		client->RFP.local_smr = dhmp_create_smr_per_ops(client->connect_trans, temp, buffer_size);
	}
	{//tailwind
		client->local_mr = client->per_ops_mr->mr;
	}
	{//FaRM
		buffer_size = sizeof(uintptr_t)*2 + sizeof(size_t)+2 + size;
		for(i = 0;i< FaRM_buffer_NUM;i++)
		{
			temp = malloc(buffer_size);
			client->FaRM.local_mr[i] = dhmp_create_smr_per_ops(client->connect_trans, temp, buffer_size)->mr;
		}
		temp = malloc(buffer_size*FaRM_buffer_NUM);
		client->FaRM.C_mr = dhmp_create_smr_per_ops(client->connect_trans, temp, buffer_size*FaRM_buffer_NUM)->mr;
	}
#ifdef pRDMA
	{
		temp = malloc(1);
		client->pRDMA.read_mr = dhmp_create_smr_per_ops(client->connect_trans, temp, 1)->mr;
		buffer_size = sizeof(uintptr_t)*2 + sizeof(size_t)+ 2 + size;
		for(i = 0;i< FaRM_buffer_NUM;i++)
		{
			temp = malloc(buffer_size);
			client->pRDMA.C_mr[i] = dhmp_create_smr_per_ops(client->connect_trans, temp, buffer_size)->mr;
		}
		
	}
#endif
	client->para_request_num = 5;
	
}

static void dhmp_close_connection(struct dhmp_transport *rdma_trans)
{
	struct dhmp_close_work close_work;
	struct dhmp_work *work;

	if(rdma_trans==NULL ||
		rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
		return ;
	
	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("allocate memory error.");
		return ;
	}

	close_work.rdma_trans=rdma_trans;
	close_work.done_flag=false;
	
	work->work_type=DHMP_WORK_CLOSE;
	work->work_data=&close_work;

	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!close_work.done_flag);

	free(work);
}

void dhmp_client_destroy()
{
	int i;
	INFO_LOG("send all disconnect start.");
#ifndef UD
	dhmp_close_connection(client->connect_trans);
#endif
	
	while(client->connect_trans->trans_state==DHMP_TRANSPORT_STATE_CONNECTED);


	client->ctx.stop=true;
	
	INFO_LOG("client destroy start.");
	pthread_join(client->ctx.epoll_thread, NULL);
	INFO_LOG("client destroy end.");
	
	free(client);
}


void model_A_write(void * globle_addr, size_t length, void * local_addr)
{
	// void * server_addr = GetAddr_request1(globle_addr, length, NULL,NULL);
	// dhmp_write(server_addr, local_addr, length);
}

void model_A_writeImm(void * globle_addr, size_t length, void * local_addr)
{
	uint32_t task_offset;
	bool cmpflag = false;
	// void * server_addr = GetAddr_request1(globle_addr, length, &task_offset, &cmpflag);
	// dhmp_write_imm(server_addr, local_addr, length, task_offset, &cmpflag); 
}

void model_B_write(void * globle_addr, size_t length, void * local_addr)
{
	dhmp_write2(globle_addr, local_addr, length);
}

void model_B_writeImm(void * globle_addr, size_t length, void * local_addr)
{
	dhmp_write_imm3(globle_addr, local_addr, length);
}

void model_B_send(void * globle_addr, size_t length, void * local_addr)
{
	dhmp_send(globle_addr, local_addr, length, 1);
}

void model_C_sread(void * globle_addr, size_t length, void * local_addr)
{
	dhmp_Sread(globle_addr, local_addr, length);
}

void model_D_write(void * server_addr, size_t length, void * local_addr)
{
	dhmp_write(server_addr, local_addr, length);
}

void model_D_writeImm(void * server_addr, size_t length, void * local_addr)
{
	void * globle_addr = server_addr; /*just one server*/
	dhmp_write_imm2(server_addr, globle_addr, local_addr, length); 
}

void model_D_send(void * server_addr, size_t length, void * local_addr)
{
	void * globle_addr = server_addr; /*just one server*/
	dhmp_send2(server_addr, globle_addr, local_addr, length); 
}

void model_1_octopus(void * globle_addr, size_t length, void * local_addr)
{
	// amper_octopus(globle_addr, globle_addr, local_addr, length); 
}
void model_1_octopus_R(void * globle_addr, size_t length, void * local_addr)
{
	GetAddr_request1(globle_addr, length, NULL,NULL); //write imm and no_lock （imm=node_id+offset）
}

void model_1_clover(void * space_addr, size_t length, void * local_addr, uintptr_t * point_addr, int offset)//+globle_obj_name
{
	dhmp_write(space_addr, local_addr, length);// local_addr = data + 0
	if(point_addr[offset] != 0) //first write
	{
		amper_clover_compare_and_set((void*)point_addr[offset], 8, (uint64_t)1);// unvalid last one
	}
	else
		point_addr[offset] = (uintptr_t)space_addr;
}
void model_1_clover_R(size_t length, void * local_addr, void* dhmp_addr)
{
	void * temp = malloc(length+8);
	if(dhmp_addr != NULL)
		dhmp_read(dhmp_addr, temp, length); // read data+point
}

void model_4_RFP( size_t length, void * local_addr, uintptr_t globle_addr, char flag_write)
{
	if(flag_write == 1)		
		amper_RFP(local_addr, globle_addr, length, true);   
	else
		amper_RFP(local_addr, globle_addr, length, false); 
}

void model_5_L5( size_t length, void * local_addr, void* globle_addr, char flag_write)
{
	amper_write_L5(local_addr, length, globle_addr, flag_write);
}

void model_3_herd(void * globle_addr, size_t length, void * local_addr)
{
	// 1000value - 2status - 16key
	// amper_write_herd(local_addr, length);  // todo
}

void model_6_Tailwind(int accessnum, int obj_num , size_t length, void * local_addr)
{
	int j = 0,i;
	size_t count = sizeof(size_t) + sizeof(uint32_t) + sizeof(uint32_t);//(size_t+check+checksum+data)
	char * tailwind_head = malloc(count);
	struct dhmp_send_mr * head_smr = dhmp_create_smr_per_ops(client->connect_trans, tailwind_head, count);
	*(size_t*)tailwind_head = length;

	dhmp_malloc(length + count, 4);
	amper_sendRPC_Tailwind(j++, head_smr->mr, count, local_addr, length);
	for(; j < accessnum-1; j++)
	{
		if(j % Tailwind_log_size == 0)
			dhmp_malloc(length + count, 4); // allocate again per log_num
		amper_write_Tailwind(j % Tailwind_log_size, head_smr->mr, count, local_addr, length); 
	}
	amper_sendRPC_Tailwind(j % Tailwind_log_size, head_smr->mr, count, local_addr, length);
	ibv_dereg_mr(head_smr->mr);
	free(head_smr);
}

void model_3_DaRPC( uintptr_t * globle_addr , size_t length, uintptr_t * local_addr, char flag_write, char batch)//server多线程未做
{
	amper_sendRPC_DaRPC(globle_addr, length, local_addr,flag_write , batch); //窗口异步 + read flush
}

//need  client->per_ops_mr_addr + client->per_ops_mr_addr2 > batch*reqsize
void model_7_scalable( uintptr_t * globle_addr , size_t length, uintptr_t * local_addr, char flag_write, char batch)
{
	int i = 0;
	size_t local_length;
	size_t offset ;
	if(client->scaleRPC.context_swith == Sca1e_Swith_Time)
	{
		//local_data
		if(flag_write == 1)
			local_length = batch * (length + sizeof(void*)*2 + sizeof(size_t) );//(data+remote_addr+size)* batch
		else
			local_length = batch * (sizeof(void*)*2 + sizeof(size_t) );//(data+remote_addr+size)* batch
		void * local_data = malloc(local_length);
		struct dhmp_send_mr * local_data_smr = dhmp_create_smr_per_ops(client->connect_trans, local_data, local_length);
		for(i = 0; i < batch ;i++)
		{
			memcpy(local_data , &(globle_addr[i]), sizeof(uintptr_t));
			memcpy(local_data + sizeof(uintptr_t) , &(local_addr[i]) , sizeof(uintptr_t));
			memcpy(local_data + sizeof(uintptr_t)*2 , &length , sizeof(size_t));
			local_data  = local_data + sizeof(uintptr_t)*2 + sizeof(size_t);
			if(flag_write == 1)
			{
				memcpy(local_data , (void*)(local_addr[i]), length);
				local_data += length;
			}
		}
		//write req-data
		size_t scalable_write_length = 4 + sizeof(size_t) + sizeof(struct ibv_mr);
		offset = batch * (length + sizeof(void*)*2 + sizeof(size_t)) - sizeof(size_t) - sizeof(struct ibv_mr);
		char * scalable_write = client->scaleRPC.Creq_mr->addr + offset;
		memcpy(scalable_write  , &local_length , sizeof(size_t));
		memcpy(scalable_write + sizeof(size_t), local_data_smr->mr , sizeof(struct ibv_mr));
		scalable_write += (sizeof(size_t) + sizeof(struct ibv_mr));
		scalable_write[0] = 1;
		scalable_write[1] = batch;
		scalable_write[2] = flag_write;
		scalable_write[3] = 1; // valid
		//post
		client->scaleRPC.context_swith = 0;
		INFO_LOG("amper_scale2 size=%d totol=%d offset = %d,dhm=%p local=%p,Creq_mr=%p  Cdata=%p",length,local_length,offset,globle_addr[0] ,
			local_addr[0],client->scaleRPC.Creq_mr->addr,client->scaleRPC.Cdata_mr->addr);
		amper_scalable(offset , scalable_write_length , 1 , length, flag_write, batch);
		ibv_dereg_mr(local_data_smr->mr);
		free(local_data_smr);
	}
	else
	{
		if(flag_write == 1)
			local_length = 4+ batch * (length + sizeof(void*)*2 + sizeof(size_t) );//(data+remote_addr+size)* batch
		else
			local_length = 4+ batch * (sizeof(void*)*2 + sizeof(size_t) );//(data+remote_addr+size)* batch
		char *scalable_write = client->scaleRPC.Creq_mr->addr;
		offset = 0;
		if(flag_write == 0)
		{
			scalable_write += (batch * length);
			offset = (batch * length);
		}
		
		for(i = 0; i < batch ;i++)
		{
			memcpy(scalable_write , &(globle_addr[i]), sizeof(uintptr_t));
			memcpy(scalable_write + sizeof(uintptr_t) , &(local_addr[i]) , sizeof(uintptr_t));
			memcpy(scalable_write + sizeof(uintptr_t)*2 , &length , sizeof(size_t));
			scalable_write  = scalable_write + sizeof(uintptr_t)*2 + sizeof(size_t);
			if(flag_write == 1)
			{
				memcpy(scalable_write , (void*)(local_addr[i]), length);
				scalable_write += length;
			}
		}
		scalable_write[0] = 2;
		scalable_write[1] = batch;
		scalable_write[2] = flag_write;
		scalable_write[3] = 1; // valid
		INFO_LOG("amper_scale2 size=%d totol=%d offset = %d,dhm=%p local=%p,Creq_mr=%p  Cdata=%p",length,local_length,offset,globle_addr[0] ,
			local_addr[0],client->scaleRPC.Creq_mr->addr,client->scaleRPC.Cdata_mr->addr);
		amper_scalable(offset, local_length , 2 , length, flag_write, batch);
		// ibv_dereg_mr(local_data_smr->mr);
		// free(local_data_smr);
	}
	client->scaleRPC.context_swith ++;
}



void send_UD(uintptr_t * globle_addr , size_t length, uintptr_t * local_addr, char flag_write, char batch)
{
	struct dhmp_UD_work wwork;
	struct dhmp_work *work;
	int i;

	/*select which node to alloc nvm memory*/
	if(!client->connect_trans)
	{
		ERROR_LOG("don't exist remote server.");
		goto out;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("allocate memory error.");
		goto out;
	}
	
	wwork.rdma_trans = client->connect_trans;
	wwork.length=length;
	wwork.done_flag=false;
	wwork.local_addr = local_addr;
	wwork.recv_flag=false;
	wwork.batch = batch;
	wwork.flag_write = flag_write;

	work->work_type=AMPER_WORK_UD;
	work->work_data=&wwork;

	while(1)
	{
		pthread_mutex_lock(&client->mutex_request_num);
		if(client->para_request_num == 0)
		{
			pthread_mutex_unlock(&client->mutex_request_num);
			continue;
		}
		client->para_request_num --;
		pthread_mutex_unlock(&client->mutex_request_num);
		break;
	}

	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!wwork.done_flag);

	free(work);
out:
	return ;
}