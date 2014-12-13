#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <set>
#include <string>
#include <cerrno>
#include <stdexcept>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <cctype>

//Main Memory Allocation Settings
#define NUM_FRAMES 1600
#define SYSTEM_FRAMES 80

//Output Settings
#define CHARS_PER_LINE 80
#define FREE_SYM '.'
#define SYSTEM_SYM '#'
#define MESG_PRECISION 4

//Frame settings
#define NO_OWNER -1
#define SYSTEM_OWNER -2


namespace MemSim
{
	//Represents an entry/exit event for a process
	struct proc_elem
	{
		bool enter_time;
		unsigned int val;
	};
	
	//Represents a process in a system
	struct process
	{
		char name;
		unsigned int mem_alloc;
		std::list<proc_elem> times;
	};
	
	//Represents a frame of memory
	struct frame
	{
		//Index of owner process
		short int owner;
	};
	
	//Used to point to free memory locations
	struct free_block
	{
		//Index of first free frame
		unsigned int start_index;
		
		//Number of blocks of free memory (>= 1)
		unsigned int size;
	};
	
	//Comparison Helper Function
	bool compare_size(const struct free_block& first, const struct free_block& second)
	{
		return (first.size <= second.size);
	}
	

	class Simulator
	{
	public:
		//Default constructor
		Simulator()
		{
			//Initialize data
			this->wall_clock = 0;
			this->next_idx = 0;
			this->quiet = false;
			this->event = false;
			this->algorithm = invalid;
			
			this->pause_clock = 1;
			
			//Initialize main memory
			for(unsigned int index = 0; index < NUM_FRAMES; index++)
			{
				struct frame new_frame;
				
				if(index < SYSTEM_FRAMES)
				{
					new_frame.owner = SYSTEM_OWNER;
				}
				else
				{
					new_frame.owner = NO_OWNER;
				}
				
				this->main_memory.push_back(new_frame);
			}
		}
		
		//Print help text for commandline arguments
		void print_help()
		{
			std::cout << std::endl
			<< "USAGE: memsim [-q] <input-file> { first | best | next | worst }" << std::endl;
			
			exit(EXIT_FAILURE);
		}
		
		//Process commandline args
		void parse_args(int argc, char ** argv)
		{
			//Check bounds
			if(argc < 3)
			{
				print_help();
			}
			else if (argc > 4)
			{
				print_help();
			}
			
			unsigned index = 1;
			
			std::string arg = argv[index];
			
			if(arg == "-q")
			{
				quiet = true;
				
				index++;
				
				arg = argv[index];
			}
			else
			{
				quiet = false;
			}
			
			//Save filename
			this->file_path = arg;
			
			//Select algorithm
			arg = argv[++index];
			
			if(arg == "first")
			{
				algorithm = first;
			}
			else if (arg == "best")
			{
				algorithm = best;
			}
			else if (arg == "next")
			{
				algorithm = next;
			}
			else if (arg == "worst")
			{
				algorithm = worst;
			}
			else if(arg == "noncontig")
			{
				algorithm = noncontig;
			}
			else
			{
				print_help();
			}
			
		}
		
		//Parse configuration file
		void parse_file()
		{
			std::ifstream file(this->file_path.c_str());
			
			if(!file)
			{
				std::cerr << "File invalid" << std::endl;
				
				print_help();
			}
			
			unsigned int N;
			
			//Get number of processes
			file >> N;
			
			file.get();
			
			//Parse process entries
			while(!file.eof())
			{
				char c;
				
				std::stringstream line;
				
				//Read to end of line
				while(((c = file.get()) != '\n') && !file.eof())
				{
					line << c;
				}
				
				//Parse line (parse process)
				struct process new_proc;
				
				//Get name
				line >> new_proc.name;
				
				//Get memory allocation
				line >> new_proc.mem_alloc;
				
				//Error-checking
				if(new_proc.mem_alloc < 10 || new_proc.mem_alloc > 100)
				{
					std::cerr << "WARNING: Process " 
					<< new_proc.name 
					<< " has a memory allocation of " 
					<< new_proc.mem_alloc 
					<< " frames. Valid range is 10 - 100." 
					<< std::endl;
				}
				
				//Parse all times
				while(line.good())
				{
					struct proc_elem entry, exit;
					
					//Get times
					line >> entry.val;
					line >> exit.val;
					
					//Set entry as an enter time
					entry.enter_time = true;
					exit.enter_time = false;
					
					//Load times into process structure
					new_proc.times.push_back(entry);
					new_proc.times.push_back(exit);
				}
				
				//Load process into class storage
				proc_list.push_back(new_proc);
			}
			
			//Error-checking
			if(N != proc_list.size())
			{
				std::cerr << "Invalid number of processes loaded!" << std::endl;
				
				exit(EXIT_FAILURE);
			}
			else if(proc_list.size() > 26)
			{
				std::cerr << "Number of processes is " 
				<< proc_list.size() 
				<< " while the valid limit is 26" 
				<< std::endl;
			}
		}
		
		//Wait for user input
		void wait_user_input()
		{
			if(this->quiet)
			{
				//Skip taking user input if running in quiet mode
				return;
			}
			
			//Prompt for user input
			std::cout << "Enter integer t, then press ENTER: ";
			
			//Read in 't' value
			std::cin >> this->pause_clock;
			
			//Check for exit command
			if(this->pause_clock == 0)
			{
				std::cout << "Exit command received" << std::endl;
				
				exit(EXIT_SUCCESS);
			}
			
		}
		
		//Determine whether an event has occured
		bool reached_event()
		{
			if((this->quiet) && (this->event)) //An event has occured (quiet)
			{
				this->event = false;
				return true;
				//TODO: return true if Process has exited or process has entered
			}
			else if(this->event && (this->wall_clock >= this->pause_clock)) //An event has occured (interactive)
			{
				this->event = false;
				return true;
			}
			else if(this->wall_clock == 0) //Initial memory map
			{
				return true;
			}
			
			return false;
		}

		//Tick clock
		void tick()
		{
			this->wall_clock++;
		}
		
		//Check for events
		void parse_events()
		{
			//Cache entry events
			std::list<unsigned int> entering_pids;
			
			//Walk through process list
			for(unsigned int pid = 0; pid < this->proc_list.size(); pid++)
			{
				//Skip processes without further events
				if(this->proc_list.at(pid).times.size() < 1)
				{
					continue;
				}
				
				//Check for event match
				if(this->proc_list.at(pid).times.front().val == this->wall_clock)
				{
					//Set event
					this->event = true;
					
					//Get event
					struct proc_elem event = this->proc_list.at(pid).times.front();
					
					//Remove event from list
					this->proc_list.at(pid).times.pop_front();
					
					//Parse event
					if(event.enter_time)
					{
						//Cache entry event
						entering_pids.push_back(pid);
					}
					else
					{
						//Execute exit event
						this->remove_process(pid);
					}
				}
			}
			
			//Execute entry events
			for(std::list<unsigned int>::const_iterator itr = entering_pids.begin(); itr != entering_pids.end(); itr++)
			{
				if(this->algorithm == noncontig)
				{
					this->insert_noncontig(*itr);
				}
				else
				{
					this->insert_contig(*itr);
				}
			}
		}
		
		//Print memory map
		void print_mem()
		{
			//Print header
			std::cout << "Memory at time " << this->wall_clock << ":" << std::endl;
			
			//Walk through main memory
			for(unsigned int index = 0; index < this->main_memory.size(); index++)
			{
				//Get special cases
				if(this->main_memory.at(index).owner == NO_OWNER)
				{
					//Frame is free
					std::cout << FREE_SYM;
				}
				else if(this->main_memory.at(index).owner == SYSTEM_OWNER)
				{
					//Frame is owned by operating system
					std::cout << SYSTEM_SYM;
				}
				else
				{
					//Frame is owned by process, find process
					unsigned int proc_idx = this->main_memory.at(index).owner;
					
					//Print process name
					std::cout << this->proc_list.at(proc_idx).name;
				}
				
				//Newline, where necessary
				if((index + 1) % CHARS_PER_LINE == 0)
				{
					std::cout << std::endl;
				}
			}
			
			std::cout << std::endl;
		}
		
		//Check for remaining events
		bool finished()
		{
			//Walk through process list
			for(std::vector<process>::const_iterator itr = this->proc_list.begin(); itr != this->proc_list.end(); itr++)
			{
				if(itr->times.size() > 0)
				{
					return false;
				}
			}
			
			return true;
		}
		
		
	protected:
		//Helper Functions
		
		
		//Find free memory blocks
		std::list<free_block> find_free()
		{
			std::list<free_block> free_list;
			
			//Walk through main memory
			for(unsigned int idx = 0; idx < this->main_memory.size(); idx++)
			{
				struct free_block new_free;
				
				//Find free block
				if((this->main_memory.at(idx)).owner == NO_OWNER)
				{
					new_free.start_index = idx;
					new_free.size = 1;
					
					//Walk to end of free block
					while((++idx < this->main_memory.size())&& ((this->main_memory).at(idx).owner == NO_OWNER))
					{
						new_free.size++;
					}
					
					free_list.push_back(new_free);
				}
				
			}
			
			return free_list;
		}
		
		//Remove process
		void remove_process(const unsigned int pid)
		{
			//Get pointer to process
			struct process * proc = &(this->proc_list.at(pid));
			
			int proc_idx = -1;
			
			//Find process in main memory
			for(unsigned int idx = 0; idx < this->main_memory.size(); idx++)
			{
				if(main_memory.at(idx).owner == (int)pid)
				{
					proc_idx = (int)idx;
					break;
				}
			}
			
			//Error-checking
			if(proc_idx < 0)
			{
				std::cerr << "Attempting to remove process not in memory" << std::endl;
			}
			
			//Get noncontig state
			bool noncontig;
			
			//Get end index
			unsigned int end_idx;
			
			if(this->algorithm == MemSim::Simulator::noncontig)
			{
				noncontig = true;
				end_idx = this->main_memory.size() - 1;
			}
			else
			{
				noncontig = false;
				end_idx = proc_idx + (proc->mem_alloc -1);
			}
			
			//Call helper function
			this->clear_helper(proc_idx, end_idx, pid, noncontig);
		}
		
		//Find best free block
		unsigned int find_best(const unsigned int num_frames)
		{
			std::list<free_block> freemem = find_free();
			
			freemem.sort(compare_size);
			
			//Find best fit
			for(std::list<free_block>::const_iterator itr = freemem.begin(); itr != freemem.end(); itr++)
			{
				if(itr->size >= num_frames)
				{
					return itr->start_index;
				}
			}
			
			//Out of memory, try defrag
			this->defragment();
			
			//Recurse
			return this->find_best(num_frames);
		}
		
		//Find worst free block
		unsigned int find_worst(const unsigned int num_frames)
		{
			std::list<free_block> freemem = find_free();
			
			freemem.sort(compare_size);
			
			//Try last (largest) free block
			if(freemem.back().size < num_frames)
			{
				//No block large enough, defragment
				this->defragment();
				
				//Recurse
				return this->find_worst(num_frames);
			}
			
			return freemem.back().start_index;
		}
		
		//Find first fitting free block
		unsigned int find_first(const unsigned int num_frames)
		{
			std::list<free_block> freemem = find_free();
			
			//Find first fit
			for(std::list<free_block>::const_iterator itr = freemem.begin(); itr != freemem.end(); itr++)
			{
				if(itr->size >= num_frames)
				{
					return itr->start_index;
				}
			}
			
			//Out of memory, try defrag
			this->defragment();
			
			//Recurse
			return this->find_first(num_frames);
		}
		
		//Helper function for frame deletion
		void clear_helper(const unsigned int start_idx, const unsigned int end_idx, const unsigned int pid, const bool is_noncontig)
		{
			//Bounds-checking
			if(start_idx < this->proc_list.size() || end_idx < this->proc_list.size())
			{
				std::cerr << "ERROR: index out of bounds" << std::endl;
				
				exit(EXIT_FAILURE);
			}
			
			for(unsigned int idx = start_idx; idx <= end_idx; idx++)
			{
				//Get frame
				struct frame curr_frame = this->main_memory.at(idx);
				
				//Error-checking
				if(!(is_noncontig))
				{
					if(curr_frame.owner != (int)pid)
					{
						std::cerr << "ERROR: memory corruption detected. Memory not contiguous" << std::endl;
						
						exit(EXIT_FAILURE);
					}
				}
				
				if(curr_frame.owner == (int)pid)
				{
					//Create new frame
					struct frame new_frame;
					
					//Set new frame as new
					new_frame.owner = NO_OWNER;
					
					//Replace original frame with new frame
					this->main_memory.at(idx) = new_frame;
				}
			}
		}
		
		//Find next fitting free block
		unsigned int find_next(const unsigned int num_frames)
		{
			std::list<free_block> freemem = find_free();
			
			//Loop 1: start from "next" position
			for(std::list<free_block>::const_iterator itr = freemem.begin(); itr != freemem.end(); itr++)
			{
				if(itr->start_index + itr->size <= this->next_idx)
				{
					continue;
				}
				
				//Handle next index being inside a free block
				if(itr->start_index < this->next_idx)
				{
					//Calculate offset size
					int adj_size = (int)(itr->size - (this->next_idx - itr->start_index));
					
					if(adj_size >= (int)num_frames)
					{
						return this->next_idx;
					}
				}
				else
				{
					if(itr->size >= num_frames)
					{
						return itr->start_index;
					}
				}
			}
			
			//Next failed, find any
			return this->find_first(num_frames);
		}
		
		//Helper function for frame "insertion"
		unsigned int insert_helper(unsigned int start_idx, const unsigned int end_idx, const unsigned int pid)
		{
			//Error-checking
			if(start_idx >= this->main_memory.size() || end_idx >= this->main_memory.size())
			{
				std::cerr << "ERROR: attempting to access out-of-bounds memory" << std::endl;
				
				exit(EXIT_FAILURE);
			}
			
			unsigned int move_count = 0;
			
			//Set next index
			this->next_idx = start_idx;
			
			while(start_idx <= end_idx)
			{
				//Error-checking
				if(this->main_memory.at(start_idx).owner != NO_OWNER)
				{
					std::cerr << "ERROR: attempting to use allocated memory" << std::endl;
					
					exit(EXIT_FAILURE);
				}
				
				//Initialize new memory element
				struct frame new_frame;
				
				//Set PID
				new_frame.owner = pid;
				
				//Insert
				this->main_memory.at(start_idx) = new_frame;
				
				move_count++;
				start_idx++;
			}
			
			return move_count;
		}
		
		//Insert contiguous process using appropriate search algorithm
		void insert_contig(const unsigned int mem_idx)
		{
			//Get process metadata
			const unsigned int num_frames = this->proc_list.at(mem_idx).mem_alloc;
			
			//Get starting index for insertion
			unsigned int start_idx; 
			
			//Switch on algorithm
			switch(this->algorithm)
			{
				case first:
					start_idx = this->find_first(num_frames);
					break;
				case best:
					start_idx = this->find_best(num_frames);
					break;
				case next:
					start_idx = this->find_next(num_frames);
					break;
				case worst:
					start_idx = this->find_worst(num_frames);
					break;
				default:
					std::cerr << "Not a valid contiguous memory allocation algorithm" << std::endl;
					
					exit(EXIT_FAILURE);
			}
			
			//Calculate ending index in main memory
			const unsigned int end_idx = start_idx + (num_frames -1);
			
			//Call helper function
			this->insert_helper(start_idx, end_idx, mem_idx);
		}
		
		//Insert noncontiguous process
		void insert_noncontig(const unsigned int mem_idx)
		{
			int space_required = (int)this->proc_list.at(mem_idx).mem_alloc; 
			
			while(space_required > 0)
			{
				std::list<free_block> freemem = find_free();
				
				//Check for out-of-memory condition
				if(freemem.size() < 1)
				{
					std::cerr << "OUT OF MEMORY" << std::endl;
					
					exit(EXIT_FAILURE);
				}
				
				//Calculate start and end insert indices
				unsigned int end_idx,
					start_idx = freemem.begin()->start_index;
				
				if(space_required > (int)freemem.begin()->size)
				{
					end_idx = start_idx + (freemem.begin()->size - 1);
				}
				else
				{
					end_idx = start_idx + (space_required - 1);
				}
				
				//Call insertion helper function
				space_required -= (int)insert_helper(start_idx, end_idx, mem_idx);
			}
		}
		
		void defragment()
		{
			//Reset next-index
			this->next_idx = 0;
			
			//Find Free Spaces
			std::list<free_block> freemem = this->find_free();
			
			//Check for free space
			if(freemem.size() == 0)
			{
				std::cerr << "ERROR: OUT OF MEMORY" << std::endl;
				
				exit(EXIT_SUCCESS);
			}
			else if(freemem.size() == 1)
			{
				//Already defragmented
				std::cerr << "ERROR: OUT OF MEMORY" << std::endl;
				
				exit(EXIT_SUCCESS);
			}
			
			//Keep metrics on number of blocks of memory moved
			std::set<int> moved_pids;
			
			std::cout << "Performing defragmentation..." << std::endl;
			
			//Begin shifting memory
			while(freemem.size() > 1)
			{
				unsigned int index = freemem.begin()->start_index + freemem.begin()->size;
				
				while(index < this->main_memory.size())
				{
					//Get owner to be moved
					int ownr = this->main_memory.at(index).owner;
					
					//Skip free and system owners
					if(ownr == NO_OWNER)
					{
						index++;
						continue;
					}
					
					//Metrics
					moved_pids.insert(ownr);
					
					//Perform swap
					struct frame fbuf;
					
					//Cache nonfree block
					fbuf = this->main_memory.at(index);
					
					//Move free block
					this->main_memory.at(index) = this->main_memory.at(index - freemem.begin()->size);
					
					//Write cached nonfree to free block's previous location
					this->main_memory.at(index - freemem.begin()->size) = fbuf;
					
					index++;
				}
				
				freemem = this->find_free();
			}
			
			//Calculate metrics
			double d_free = (double)freemem.begin()->size;
			double d_total = (double)(this->main_memory.size());
			
			double percent = 100 * (d_free / d_total);
			
			//Set cout precision
			std::cout.precision(MESG_PRECISION);
			
			//Print Results
			std::cout << "Defragmentation completed."
			<< std::endl
			<< "Relocated "
			<< moved_pids.size()
			<< " processes to create a free memory block of "
			<< freemem.begin()->size
			<< " units ("
			<< percent
			<< "% of total memory)."
			<< std::endl
			<< std::endl;
		}
		
	
	private:
		//Storage Structures
		std::vector<process> proc_list; //List of processes/events
		std::vector<frame> main_memory; //Main memory frames
		
		//Possible allocation algorithms
		enum alloc_mode
		{
			first,
			best,
			next,
			worst,
			noncontig,
			invalid, //Indicates init error
		};
		
		//Sets which algorithm to use
		alloc_mode algorithm;
		
		//Determines whether sim runs in quiet mode
		bool quiet;
		
		//Starting index of last-used memory block (for NEXT algorithm)
		unsigned int next_idx;
		
		//Path to config file
		std::string file_path;
		
		//Simulation Runtime Settings===================
		unsigned long long int wall_clock, //Current clock time
				pause_clock; //Time at which to pause
			
		//Indicates an event has occured
		bool event;
	};

}

int main(int argc, char **argv) {
    
	MemSim::Simulator sim;
	
	//Initialize simulator
	sim.parse_args(argc, argv);
	
	sim.parse_file();
	
	//Begin main loop
	while(!sim.finished())
	{
		//Run memory event operations
		sim.parse_events();
		
		//Check for event
		if(sim.reached_event())
		{
			sim.print_mem();
			
			sim.wait_user_input();
		}
		
		//Increment clock cycle
		sim.tick();
	}
	
	//Print final results
	sim.print_mem();
	
	std::cout << "End of Simulation" << std::endl;
	
	return EXIT_SUCCESS;
}
