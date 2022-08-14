#include "sa_sched.h"

// Addby LCL7. for new constrain
#define NEW_CONSTRAIN_INSERT_CHECK_DEBUG 0

extern struct sched_set tsn_sched_set;
extern struct global_resource g_resource;
extern int loop_num;
extern double rand_prob;

// Add by LCL7. --> for new constrain
extern struct flow_task_set tsn_flow_task_set;

static struct sa_sched_solution candidate_solution = {0};
static struct sa_sched_solution init_solution = {0};
static struct sa_sched_solution cur_solution = {0};

static int generate_init_solution_random()
{
    int i = 0, j = 0, s = 0, t = 0;
    int offset = 0;
    u8 node = 0;
    u8 port = 0;
    u16 slot = 0;
    u8 src_node = 0;
    int flag = 1;

    // Add by LCL7. --> temp offset value for new constrain check
    int lcl7_count =0;
    int tmp_offset = 0;

    for(j = 0; j < tsn_sched_set.cur_flow_num; j++)
    {
        if(tsn_sched_set.sched[j].flag == SUCCESS)
            continue;
//REPEAT:            
        if(random(2) == 1)      // Random 50% of all flows. by LCL7
            offset = random(tsn_sched_set.sched[j].period_slot);
        else
            continue;
            
        flag = 0;
        src_node = tsn_sched_set.sched[j].src_sw_id;
        for(s = 0; s < tsn_sched_set.sched[j].path_len; s++)
        {
            node = tsn_sched_set.sched[j].path_info[s].sw_id;
            port = tsn_sched_set.sched[j].path_info[s].port_id;
            for(t = 0; t < (g_resource.cur_sched_slot_num / tsn_sched_set.sched[j].period_slot); t++)
            {
                slot = (offset + t * tsn_sched_set.sched[j].period_slot + s + 1) % g_resource.cur_sched_slot_num;
                g_resource.cqf[node][port][slot].free_len -= tsn_sched_set.sched[j].pkt_num;
                g_resource.cqf[node][port][slot].used_len += tsn_sched_set.sched[j].pkt_num;
                if(g_resource.cqf[node][port][slot].used_len > CQF_QUEUE_LEN)
                {
                    flag = 1;
                }
            }
        }

        // Add by LCL7. --> initial insert check for new constrain
        if (flag != 1)
        {
            tmp_offset = tsn_sched_set.sched[j].offset;
            tsn_sched_set.sched[j].offset = offset;
            if(sa_new_constrain_insert_check(&tsn_sched_set.sched[j], &tsn_sched_set.sched[0], tsn_sched_set.cur_flow_num) != 0)
            {
                flag = 1;
                tsn_sched_set.sched[j].offset = tmp_offset;
                tsn_sched_set.sched[j].flag = FAIL;
                // printf("--> %s: init insert fail!\n", __func__);
            }
            lcl7_count += 1;
        }
        
        if(flag == 1)
        {
            for(s = 0; s < tsn_sched_set.sched[j].path_len; s++)
            {
                node = tsn_sched_set.sched[j].path_info[s].sw_id;
                port = tsn_sched_set.sched[j].path_info[s].port_id;
                for(t = 0; t < (g_resource.cur_sched_slot_num / tsn_sched_set.sched[j].period_slot); t++)
                {
                    slot = (offset + t * tsn_sched_set.sched[j].period_slot + s + 1) % g_resource.cur_sched_slot_num;
                    g_resource.cqf[node][port][slot].free_len += tsn_sched_set.sched[j].pkt_num;
                    g_resource.cqf[node][port][slot].used_len -= tsn_sched_set.sched[j].pkt_num;
                }
            }
 //           goto REPEAT;
        }
        else
        {
            tsn_sched_set.sched[j].flag = SUCCESS;
            tsn_sched_set.sched[j].offset = offset;
            tsn_sched_set.cur_suc_num++;
        }

    }

    memset(&init_solution, 0, sizeof(struct sa_sched_solution));   // Fixed by LCL7. 21.11.28
    for(i = 0; i < tsn_sched_set.cur_flow_num; i++)
    {
        if(tsn_sched_set.sched[i].flag == SUCCESS)
        {
            init_solution.sched_suc[init_solution.cur_suc_num] = tsn_sched_set.sched[i];
            init_solution.cur_suc_num++;
        }
        else
        {
            init_solution.sched_fail[init_solution.cur_fail_num] = tsn_sched_set.sched[i];
            init_solution.cur_fail_num++;
        }
    }
    init_solution.g_resource = g_resource;
    cur_solution = init_solution;

     // Changed by LCL7. --> for new constrain test print
    printf("new_constrained init solution: %d, original init solution: %d, tsn_sched_set.cur_suc_num: %d \n", init_solution.cur_suc_num, lcl7_count, tsn_sched_set.cur_suc_num);
}


static int generate_init_solution_pro()
{
    int i = 0;
    memset(&init_solution, 0, sizeof(struct sa_sched_solution));
    for(i = 0; i < tsn_sched_set.cur_flow_num; i++)
    {
        if(tsn_sched_set.sched[i].flag == SUCCESS)
        {
            init_solution.sched_suc[init_solution.cur_suc_num] = tsn_sched_set.sched[i];
            init_solution.cur_suc_num++;
        }
        else
        {
            init_solution.sched_fail[init_solution.cur_fail_num] = tsn_sched_set.sched[i];
            init_solution.cur_fail_num++;
        }
    }
    init_solution.g_resource = g_resource;
    cur_solution = init_solution;
//    printf("init solution: %d\n", init_solution.cur_suc_num);
}


static void generate_candidate_solution_tradeoff()
{
    int i = 0, j = 0, k = 0, s = 0;
    int num = 0;
    int start = 0;
    struct sched_info temp = {0};
    u8 src_node  = 0;
    u16 node = 0;
    u16 port = 0;
    u16 slot = 0;
    u32 temp_num = 0;
    u32 exchange = 0;
    u32 switch_one = 0;
    u32 switch_two = 0;
    double pick = 0;
    int pro_num = 0;

    candidate_solution = cur_solution;
    pick = ((double)rand()) / RAND_MAX;

    /*-- Exchange Mode Step 1: remove some flows from current solution. by LCL7 --*/
    if(pick > rand_prob)
    {// DSK strategy. by LCL7
        // Sorted by multiple flow feature. by LCL7
        for(i = 0; i < SA_STEP_SIZE; i++)
        {
            for(j = 0; j < candidate_solution.cur_suc_num - i - 1; j++)
            {
                if(user_defined_multi_stage_sort(candidate_solution.sched_suc[j], candidate_solution.sched_suc[j + 1]))
    //            if(candidate_solution.sched_suc[j].pkt_num > candidate_solution.sched_suc[j + 1].pkt_num)
    //            if(candidate_solution.sched_suc[j].period_slot < candidate_solution.sched_suc[j + 1].period_slot)
    //            if(candidate_solution.sched_suc[j].deadline_slot < candidate_solution.sched_suc[j + 1].deadline_slot)
    //            if(candidate_solution.sched_suc[j].path_len > candidate_solution.sched_suc[j + 1].path_len)
                {
                    temp = candidate_solution.sched_suc[j];
                    candidate_solution.sched_suc[j] = candidate_solution.sched_suc[j + 1];
                    candidate_solution.sched_suc[j + 1] = temp;
                }
            }
        }
    }
    else
    {// Random strategy. by LCL7
        for(i = 0; i < candidate_solution.cur_suc_num; i++)
        { 
            exchange = random(candidate_solution.cur_suc_num);
            temp = candidate_solution.sched_suc[candidate_solution.cur_suc_num - 1 - i];
            candidate_solution.sched_suc[candidate_solution.cur_suc_num - 1 - i] = candidate_solution.sched_suc[exchange];
            candidate_solution.sched_suc[exchange] = temp;
    //        printf("i: %d, flow_id: %d\n", i, cur_solution.sched_suc[i].flow_id);
        }
    }

    for(i = 0; i < SA_STEP_SIZE; i++)
    {
        // Changed by LCL7. --> for extremely low probability or low flow number
        if (candidate_solution.cur_suc_num >= STEP_SIZE)
        {
            start = candidate_solution.cur_suc_num - SA_STEP_SIZE + i;
        }
        else
        {
            start = i;
            if (start >= candidate_solution.cur_suc_num)
                break;
        }
        
//            printf("candidate: %d, out: %d\n", num, candidate_set[num].entry.out[i]);
        candidate_solution.sched_suc[start].flag = FAIL;
        candidate_solution.sched_fail[candidate_solution.cur_fail_num + i] = candidate_solution.sched_suc[start];
        src_node = candidate_solution.sched_suc[start].src_sw_id;
        for(j = 0; j < candidate_solution.sched_suc[start].path_len; j++)
        {
            node = candidate_solution.sched_suc[start].path_info[j].sw_id;
            port = candidate_solution.sched_suc[start].path_info[j].port_id;
            for(k = 0; k < (candidate_solution.g_resource.cur_sched_slot_num / candidate_solution.sched_suc[start].period_slot); k++)
            {
                slot = (candidate_solution.sched_suc[start].offset + k * candidate_solution.sched_suc[start].period_slot + j + 1) \
                       % candidate_solution.g_resource.cur_sched_slot_num;
                candidate_solution.g_resource.cqf[node][port][slot].free_len += candidate_solution.sched_suc[start].pkt_num;
                candidate_solution.g_resource.cqf[node][port][slot].used_len -= candidate_solution.sched_suc[start].pkt_num;
            }
        }
    }
    
    // Changed by LCL7. --> for extremely low probability or low flow number
    if (candidate_solution.cur_suc_num >= STEP_SIZE)
    {
        candidate_solution.cur_suc_num -= STEP_SIZE;
        candidate_solution.cur_fail_num += STEP_SIZE;
    }
    else
    {
        candidate_solution.cur_suc_num  = 0;
        candidate_solution.cur_fail_num += candidate_solution.cur_suc_num;
    }
    if(candidate_solution.cur_fail_num < STEP_SIZE)
    {
        printf("The step size exceed the flow fail set size\n");
    }

    /*-- Exchange Mode Step 2: insert some flows from current solution. by LCL7 --*/
    if(pick > rand_prob)
    {// DSK strategy. by LCL7
        for(i = 0; i < SA_STEP_SIZE; i++)
        //for(i = 0; i < candidate_solution.cur_fail_num - 1; i++)
        {
            for(j = candidate_solution.cur_fail_num - 2; j >= i; j--)
            {
                if(user_defined_multi_stage_sort(candidate_solution.sched_fail[j], candidate_solution.sched_fail[j + 1]))
                {
                    temp = candidate_solution.sched_fail[j];
                    candidate_solution.sched_fail[j] = candidate_solution.sched_fail[j + 1];
                    candidate_solution.sched_fail[j + 1] = temp;
                }
            }
        }
        
        for(i = 0; i < candidate_solution.cur_fail_num; i++)
        {
            insert_flow_with_adjust_offset_density(&candidate_solution.sched_fail[i], &candidate_solution.g_resource);
    //        insert_flow_with_adjust_offset_descend(&candidate_solution.sched_fail[i], &candidate_solution.g_resource);
            
            // Add by LCL7. --> sa insert check for new constrain
            if(candidate_solution.sched_fail[i].flag == SUCCESS)
            {
                // printf("--> %s: candidate_solution.sched_fail[i]= %d \n", __func__, candidate_set[num].sched_fail[i].offset);
                if (sa_new_constrain_insert_check(&candidate_solution.sched_fail[i], &candidate_solution.sched_suc[0], candidate_solution.cur_suc_num) != 0)
                {
                    candidate_solution.sched_fail[i].flag = FAIL;
                    candidate_solution.sched_fail[i].offset = 0;
                }
            }
            
            if(candidate_solution.sched_fail[i].flag == SUCCESS)
            {
                candidate_solution.sched_suc[candidate_solution.cur_suc_num] = candidate_solution.sched_fail[i];
                candidate_solution.cur_suc_num++;
            }
        }
 
    }
    else
    {// Random strategy. by LCL7
        for(i = 0; i < candidate_solution.cur_fail_num; i++)
        {
            switch_one = random(candidate_solution.cur_fail_num);
            switch_two = random(candidate_solution.cur_fail_num);
            temp = candidate_solution.sched_fail[switch_one];
            candidate_solution.sched_fail[switch_one] = candidate_solution.sched_fail[switch_two];
            candidate_solution.sched_fail[switch_two] = temp;
        }
        
        for(i = 0; i < candidate_solution.cur_fail_num; i++)
        {
            insert_flow_with_adjust_offset_random(&candidate_solution.sched_fail[i], &candidate_solution.g_resource);
     //       insert_flow_with_adjust_offset_descend(&candidate_solution.sched_fail[i], &candidate_solution.g_resource);
            
            // Add by LCL7. --> sa insert check for new constrain
            if(candidate_solution.sched_fail[i].flag == SUCCESS)
            {
                // printf("--> %s: candidate_solution.sched_fail[i]= %d \n", __func__, candidate_set[num].sched_fail[i].offset);
                if (sa_new_constrain_insert_check(&candidate_solution.sched_fail[i], &candidate_solution.sched_suc[0], candidate_solution.cur_suc_num) != 0)
                {
                    candidate_solution.sched_fail[i].flag = FAIL;
                    candidate_solution.sched_fail[i].offset = 0;
                }
            }
            
            if(candidate_solution.sched_fail[i].flag == SUCCESS)
            {
                candidate_solution.sched_suc[candidate_solution.cur_suc_num] = candidate_solution.sched_fail[i];
                candidate_solution.cur_suc_num++;
            }
        }
    }

    /*-- Exchange Mode Step 3: update current cadidate solution. by LCL7 --*/
    j = 0;
    i = 0;

    temp_num = candidate_solution.cur_fail_num;
    while(i < candidate_solution.cur_fail_num)
    {
        if(candidate_solution.sched_fail[i].flag == SUCCESS)
        {
            i++;
            temp_num--;
        }
        else
        {
            if(j < i)
            {
                candidate_solution.sched_fail[j] = candidate_solution.sched_fail[i];
            }

            j++;
            i++;
        }
    }
    candidate_solution.cur_fail_num = temp_num;
}

int sa_sched_out_tradeoff()
{
    int i = 0, j = 0;
    int repeat_num = 0;
    double cur_temp = MAX_TEMP;
    int repeat_flag = 1;
    int df = 0;
	loop_num = 0;
//	struct sa_sched_solution pre_solution = {0};
	struct sa_sched_solution best_solution = {0};

    // Add by LCL7. --> for new constrain analysis
    NEW_CONSTRAIN_INSERT_CHECK_CALL_COUNT = 0;
    NEW_CONSTRAIN_INSERT_CHECK_FLOW_PAIR_COUNT = 0;
	
    generate_init_solution_random();                    // Need to add new constrain. by LCL7

    // Add by LCL7. --> new constrain test for initial solution, just to test unexpected error
    // printf("--> %s: test 1 fail_num= %d\n", __func__, new_constrain_result_check(tsn_flow_task_set.cur_slot_cycle));

	best_solution = init_solution;

    while(cur_temp > MIN_TEMP)
    {
//        printf("cur_solution.g_resource.sched_slot_num: %d\n", cur_solution.g_resource.cur_sched_slot_num);
        if(cur_solution.cur_suc_num == tsn_sched_set.cur_flow_num)
            break;

//		pre_solution = cur_solution;
        for(i = 0; i < SA_CANDIDATE_NUM; i++)
        {
            generate_candidate_solution_tradeoff();     // Need to add new constrain. by LCL7
            if(candidate_solution.cur_suc_num >= cur_solution.cur_suc_num)
            {
                cur_solution = candidate_solution;
            }
            else
            {
                df = candidate_solution.cur_suc_num - cur_solution.cur_suc_num;
                if(exp((double)df/cur_temp) > ((double)rand())/RAND_MAX)
                {
                    cur_solution = candidate_solution;
                }
            }
			if(cur_solution.cur_suc_num > best_solution.cur_suc_num)
				best_solution = cur_solution;
        }
        if(best_solution.cur_suc_num == cur_solution.cur_suc_num)
        {
        	repeat_num++;
            if(repeat_num >= SA_MAX_REPEAT_NUM)
            {
            	repeat_flag = 0;
                printf("repeat flag: %d\n", repeat_flag);       // Add by LCL7.
                break;
            }
        }
        else
            repeat_num = 0;

/*
        if(repeat_flag == 0)
        {
			printf("repeat flag: %d\n", repeat_flag);
            break;
        }
*/		
        cur_temp = cur_temp * ANNEAL_PARA;
		loop_num++;
    }

	cur_solution = best_solution;
    g_resource = cur_solution.g_resource;
    for(i = 0; i < tsn_sched_set.cur_flow_num; i++)
    {
        tsn_sched_set.sched[i].flag = FAIL;
    }

    for(i = 0; i < cur_solution.cur_suc_num; i++)
    {
        for(j = 0; j < tsn_sched_set.cur_flow_num; j++)
        {
            if(cur_solution.sched_suc[i].flow_id == tsn_sched_set.sched[j].flow_id)
            {
                tsn_sched_set.sched[j] = cur_solution.sched_suc[i];
                break;
            }
        }
    }
    tsn_sched_set.cur_suc_num = cur_solution.cur_suc_num;
}

/***************************************************
 * Add by LCL7 -> insert check for new constrain   *
 *  SA  Algorithm Version                          *
 * IF SUCCESS THEN return 0; OTHERWISE return -1.  *
 ***************************************************
 */
int sa_new_constrain_insert_check(struct sched_info *p_check_sched, struct sched_info *p_can_sched_head, int can_flow_num)
{
    NEW_CONSTRAIN_INSERT_CHECK_CALL_COUNT += 1;
    if (ENDSYSTEM_CONSTRAIN_INSERT_CHECK_FLAG == 0)
    {
        return 0;
    }
    
    int i=0, j;     // i just keep for test
    int a, b;
    int cur_slot_cycle;
    int cur_case_flag;
    int case_value, cur_lcm_value;
    int cur_rx_wcet_i, cur_rx_wcet_j;
    int cur_tx_wcet_i, cur_tx_wcet_j;
    struct sched_set cur_sched_set;
    struct sched_info cur_sched_i, cur_sched_j;
    int remove_flow_num;

    int case1_num, case2_num, case3_4_num;
    int case1_fail_num, case2_fail_num, case3_4_fail_num;

    memset(&cur_sched_set, 0, sizeof(struct sched_set));
    memset(&cur_sched_i, 0, sizeof(struct sched_info));
    memset(&cur_sched_j, 0, sizeof(struct sched_info));
    cur_sched_set.cur_flow_num = 2;

    if (NEW_CONSTRAIN_INSERT_CHECK_DEBUG)
    {
        printf("--> %s: begin ...\n", __func__);
    }
    case1_num = 0;
    case2_num = 0;
    case3_4_num = 0;
    case1_fail_num = 0;
    case2_fail_num = 0;
    case3_4_fail_num = 0;
    remove_flow_num = 0;

    // if (p_check_sched->flag != SUCCESS)
    // {
    //     printf("--> %s: unexpected error! \n", __func__);
    //     return -1;
    // }

    memcpy(&cur_sched_i, p_check_sched, sizeof(struct sched_info));
    memcpy(&(cur_sched_set.sched[0]), &cur_sched_i, sizeof(struct sched_info));
    cur_rx_wcet_i = tsn_flow_task_set.flow_task[cur_sched_i.flow_id].rx_wcet;
    cur_tx_wcet_i = tsn_flow_task_set.flow_task[cur_sched_i.flow_id].tx_wcet;

    cur_slot_cycle = tsn_flow_task_set.cur_slot_cycle;

    for (j = 0; j < can_flow_num; j++)
    {
        memcpy(&cur_sched_j, p_can_sched_head + j, sizeof(struct sched_info));

        if (cur_sched_j.flag == FAIL || cur_sched_j.flow_id == cur_sched_i.flow_id)
        {
            // printf("--> FAIL flag\n");
            continue;
        }

        NEW_CONSTRAIN_INSERT_CHECK_FLOW_PAIR_COUNT += 1;

        case_value = (cur_sched_i.dst_sw_id == cur_sched_j.dst_sw_id) ? 1 : \
                            (cur_sched_i.src_sw_id == cur_sched_j.src_sw_id) ? 2 : \
                            (cur_sched_i.dst_sw_id == cur_sched_j.src_sw_id) ? 3 : \
                            (cur_sched_i.src_sw_id == cur_sched_j.dst_sw_id) ? 4: 0;
        if (case_value == 0)
        {
            continue;
        }
	
        memcpy(&(cur_sched_set.sched[1]), &cur_sched_j, sizeof(struct sched_info));
        cur_lcm_value = lcm(cur_sched_set);
        cur_rx_wcet_j = tsn_flow_task_set.flow_task[cur_sched_j.flow_id].rx_wcet;
        cur_tx_wcet_j = tsn_flow_task_set.flow_task[cur_sched_j.flow_id].tx_wcet;

        cur_case_flag = SUCCESS;

        switch (case_value)
        {
            case 1:     // both rx tasks
                case1_num += 1;
                for(a = 0; a < cur_lcm_value; a++)
                {
                    for(b = 0; b < cur_lcm_value; b++)
                    {
                        if ((a*cur_sched_i.period_slot + cur_sched_i.offset + cur_sched_i.path_len) == (b*cur_sched_j.period_slot + cur_sched_j.offset + cur_sched_j.path_len))
                        {// both rx task in the same slot
                            if (cur_rx_wcet_i + cur_rx_wcet_j + 2*DetChipISR > cur_slot_cycle)
                            {
                                case1_fail_num += 1;
                                cur_case_flag = FAIL;
                            }
                            
                        }
                        else if (((a*cur_sched_i.period_slot + cur_sched_i.offset + cur_sched_i.path_len + (u16)ceil((cur_rx_wcet_i + (u16)(DetChipISR)) / (double)cur_slot_cycle) <= b*cur_sched_j.period_slot + cur_sched_j.offset + cur_sched_j.path_len) \
                            || (b*cur_sched_j.period_slot + cur_sched_j.offset + cur_sched_j.path_len + (u16)ceil((cur_rx_wcet_j + (u16)(DetChipISR)) / (double)cur_slot_cycle) <= a*cur_sched_i.period_slot + cur_sched_i.offset + cur_sched_i.path_len)) \
                                == FALSE)
                        {   
                            case1_fail_num += 1;
                            cur_case_flag = FAIL;
                        }
                        
                        if (cur_case_flag == FAIL)
                        {
                            break;
                        }
                    }
                    
                    if (cur_case_flag == FAIL)
                    {
                        if (NEW_CONSTRAIN_INSERT_CHECK_DEBUG)
                        {
                            printf("--> %s: case 1 ...\n", __func__);
                            printf("i: %d, j: %d, a: %d, b: %d\n", i, j, a, b);
                            printf("f_i -> flow_id: %d, flag: %d, src_sw_id: %d, dst_sw_id: %d, period_slot: %d, offset: %d, path_len: %d, rx_wcet_slot: %d, tx_wcet_slot: %d, rx_wcet: %d, tx_wcet: %d \n", cur_sched_i.flow_id, cur_sched_i.flag, cur_sched_i.src_sw_id, cur_sched_i.dst_sw_id, cur_sched_i.period_slot, cur_sched_i.offset, cur_sched_i.path_len, (u16)ceil(cur_rx_wcet_i / (double)cur_slot_cycle), (u16)ceil(cur_tx_wcet_i / (double)cur_slot_cycle), cur_rx_wcet_i, cur_tx_wcet_i);
                            printf("f_j -> flow_id: %d, flag: %d, src_sw_id: %d, dst_sw_id: %d, period_slot: %d, offset: %d, path_len: %d, rx_wcet_slot: %d, tx_wcet_slot: %d, rx_wcet: %d, tx_wcet: %d \n", cur_sched_j.flow_id, cur_sched_j.flag, cur_sched_j.src_sw_id, cur_sched_j.dst_sw_id, cur_sched_j.period_slot, cur_sched_j.offset, cur_sched_j.path_len, (u16)ceil(cur_rx_wcet_j / (double)cur_slot_cycle), (u16)ceil(cur_tx_wcet_j / (double)cur_slot_cycle), cur_rx_wcet_j, cur_tx_wcet_j);
                        }
                        break;
                    }
                    
                }
                break;
            
            case 2:     // both tx tasks
                case2_num += 1;
                for(a = 0; a < cur_lcm_value; a++)
                {
                    for(b = 0; b < cur_lcm_value; b++)
                    {
                        if ((a*cur_sched_i.period_slot + cur_sched_i.offset) == (b*cur_sched_j.period_slot + cur_sched_j.offset))
                        {
                            if (cur_tx_wcet_i + cur_tx_wcet_j + 2*DetChipISR > cur_slot_cycle)
                            {
                                case2_fail_num += 1;
                                cur_case_flag = FAIL;
                            }                      
                        }
                        else if (((a*cur_sched_i.period_slot + cur_sched_i.offset + (u16)ceil((cur_tx_wcet_j + (u16)(DetChipISR)) / (double)cur_slot_cycle) <= b*cur_sched_j.period_slot + cur_sched_j.offset) \
                            || (b*cur_sched_j.period_slot + cur_sched_j.offset + (u16)ceil((cur_tx_wcet_i + (u16)(DetChipISR)) / (double)cur_slot_cycle) <= a*cur_sched_i.period_slot + cur_sched_i.offset)) \
                                == FALSE)
                        {
                            case2_fail_num += 1;
                            cur_case_flag = FAIL;
                        }

                        if (cur_case_flag == FAIL)
                        {
                            break;
                        }
                    }
                    if (cur_case_flag == FAIL)
                    {
                        if (NEW_CONSTRAIN_INSERT_CHECK_DEBUG)
                        {
                            printf("--> %s: case 2 ...\n", __func__);
                            printf("i: %d, j: %d, a: %d, b: %d\n", i, j, a, b);
                            printf("f_i -> flow_id: %d, flag: %d, src_sw_id: %d, dst_sw_id: %d, period_slot: %d, offset: %d, path_len: %d, rx_wcet_slot: %d, tx_wcet_slot: %d, rx_wcet: %d, tx_wcet: %d \n", cur_sched_i.flow_id, cur_sched_i.flag, cur_sched_i.src_sw_id, cur_sched_i.dst_sw_id, cur_sched_i.period_slot, cur_sched_i.offset, cur_sched_i.path_len, (u16)ceil(cur_rx_wcet_i / (double)cur_slot_cycle), (u16)ceil(cur_tx_wcet_i / (double)cur_slot_cycle), cur_rx_wcet_i, cur_tx_wcet_i);
                            printf("f_j -> flow_id: %d, flag: %d, src_sw_id: %d, dst_sw_id: %d, period_slot: %d, offset: %d, path_len: %d, rx_wcet_slot: %d, tx_wcet_slot: %d, rx_wcet: %d, tx_wcet: %d \n", cur_sched_j.flow_id, cur_sched_j.flag, cur_sched_j.src_sw_id, cur_sched_j.dst_sw_id, cur_sched_j.period_slot, cur_sched_j.offset, cur_sched_j.path_len, (u16)ceil(cur_rx_wcet_j / (double)cur_slot_cycle), (u16)ceil(cur_tx_wcet_j / (double)cur_slot_cycle), cur_rx_wcet_j, cur_tx_wcet_j);
                        }
                        break;
                    }
                }
                break;
            
            case 3:     // rx task before tx task
            case 4:     // tx task before rx task
                case3_4_num += 1;
                for(a = 0; a < cur_lcm_value; a++)
                {
                    for(b = 0; b < cur_lcm_value; b++)
                    {
                        if ((a*cur_sched_i.period_slot + cur_sched_i.offset + cur_sched_i.path_len) == (b*cur_sched_j.period_slot + cur_sched_j.offset) \
                            || (a*cur_sched_i.period_slot + cur_sched_i.offset) == (b*cur_sched_j.period_slot + cur_sched_j.offset + cur_sched_j.path_len))
                        {
                            if (((cur_sched_i.dst_sw_id == cur_sched_j.src_sw_id) && (cur_rx_wcet_i + cur_tx_wcet_j + 2*DetChipISR > cur_slot_cycle)) \
                                || ((cur_sched_i.src_sw_id == cur_sched_j.dst_sw_id) && (cur_tx_wcet_i + cur_rx_wcet_j + 2*DetChipISR > cur_slot_cycle)))
                            {
                                case3_4_fail_num += 1;
                                cur_case_flag = FAIL;
                            }
                            
                        }
                        else{
                            if (cur_sched_i.dst_sw_id == cur_sched_j.src_sw_id)
                            {// case 3
                                if (((a*cur_sched_i.period_slot + cur_sched_i.offset + cur_sched_i.path_len + (u16)ceil((cur_rx_wcet_i + cur_tx_wcet_j + (u16)(DetChipISR)) / (double)cur_slot_cycle) <= b*cur_sched_j.period_slot + cur_sched_j.offset) \
                                    || (b*cur_sched_j.period_slot + cur_sched_j.offset + (u16)ceil((u16)(DetChipISR) / (double)cur_slot_cycle) <= a*cur_sched_i.period_slot + cur_sched_i.offset + cur_sched_i.path_len)) \
                                    == FALSE)
                                {
                                    case3_4_fail_num += 1;
                                    cur_case_flag = FAIL;
                                }
                            }
                            
                            if (cur_case_flag != FAIL && cur_sched_i.src_sw_id == cur_sched_j.dst_sw_id)
                            {// case 4 (sometimes both case 3 and case 4)
                                if (((b*cur_sched_j.period_slot + cur_sched_j.offset + cur_sched_j.path_len + (u16)ceil((cur_rx_wcet_j + cur_tx_wcet_i + (u16)(DetChipISR)) / (double)cur_slot_cycle) <= a*cur_sched_i.period_slot + cur_sched_i.offset) \
                                    || (a*cur_sched_i.period_slot + cur_sched_i.offset + (u16)ceil((u16)(DetChipISR) / (double)cur_slot_cycle) <= b*cur_sched_j.period_slot + cur_sched_j.offset + cur_sched_j.path_len)) \
                                    == FALSE)
                                {
                                    if (cur_case_flag != FAIL)
                                    {
                                        case3_4_fail_num += 1;
                                    }
                                    cur_case_flag = FAIL;
                                }
                            }
                        }

                        if (cur_case_flag == FAIL)
                        {
                            break;
                        }
                    }
                    if (cur_case_flag == FAIL)
                    {
                        if (NEW_CONSTRAIN_INSERT_CHECK_DEBUG)
                        {
                            printf("--> %s: case 3/4 ...\n", __func__);
                            printf("i: %d, j: %d, a: %d, b: %d\n", i, j, a, b);
                            printf("f_i -> flow_id: %d, flag: %d, src_sw_id: %d, dst_sw_id: %d, period_slot: %d, offset: %d, path_len: %d, rx_wcet_slot: %d, tx_wcet_slot: %d, rx_wcet: %d, tx_wcet: %d \n", cur_sched_i.flow_id, cur_sched_i.flag, cur_sched_i.src_sw_id, cur_sched_i.dst_sw_id, cur_sched_i.period_slot, cur_sched_i.offset, cur_sched_i.path_len, (u16)ceil(cur_rx_wcet_i / (double)cur_slot_cycle), (u16)ceil(cur_tx_wcet_i / (double)cur_slot_cycle), cur_rx_wcet_i, cur_tx_wcet_i);
                            printf("f_j -> flow_id: %d, flag: %d, src_sw_id: %d, dst_sw_id: %d, period_slot: %d, offset: %d, path_len: %d, rx_wcet_slot: %d, tx_wcet_slot: %d, rx_wcet: %d, tx_wcet: %d \n", cur_sched_j.flow_id, cur_sched_j.flag, cur_sched_j.src_sw_id, cur_sched_j.dst_sw_id, cur_sched_j.period_slot, cur_sched_j.offset, cur_sched_j.path_len, (u16)ceil(cur_rx_wcet_j / (double)cur_slot_cycle), (u16)ceil(cur_tx_wcet_j / (double)cur_slot_cycle), cur_rx_wcet_j, cur_tx_wcet_j);
                        }
                        break;
                    }
                }
                break;

            default:
                break;
        }

        if (cur_case_flag == FAIL)
        {
            remove_flow_num += 1;
            break;
        }
    }

    if (NEW_CONSTRAIN_INSERT_CHECK_DEBUG)
    {
        printf("--> %s: remove_flow_num: %d \n", __func__, remove_flow_num);
        printf("--> %s: case1: %d, case2: %d, case3/4: %d\n", __func__, case1_num, case2_num, case3_4_num);
        printf("--> %s: case1_fail_num: %d, case2_fail_num: %d, case3_4_fail_num: %d\n", __func__, case1_fail_num, case2_fail_num, case3_4_fail_num);
        printf("--> %s: end...\n", __func__);
    }
    
    return remove_flow_num;
}
