import csv
from collections import Counter

#initialize data arrays
super_data = []
group_data = []
bitmap_data = []
inode_data = []
directory_data = []
indirect_data = []

#open lab3b_check.txt
f = open('lab3b_check.txt', 'w+')

def read_inode_data(x):
    for row in inode_data:
        for z in range(11,26):
            if int(row[z],16) == x:
                f.write("UNALLOCATED BLOCK < " + str(x) + " > REFERENCED BY INODE < " + row[0] + " > ENTRY < " + str(z - 11) + " >\n")
                return

def read_duplicate_inode_data(x):
    f.write("MULTIPLY REFERENCED BLOCK < " + str(x) + " > BY")
    for row in inode_data:
        for z in range(11,26):
            if int(row[z],16) == x:
                f.write(" INODE < " + row[0] + " > ENTRY < " + str(z - 11) + " >")
    f.write("\n")

def main():
    #read data from csv files
    with open('super.csv', 'r') as csvfile:
        csv_reader = csv.reader(csvfile, delimiter=',')
        for row in csv_reader:
            super_data.append(row)
    with open('group.csv', 'r') as csvfile:
        csv_reader = csv.reader(csvfile, delimiter=',')
        for row in csv_reader:
            temp_list = []
            for value in row:
                temp_list.append(value)
            group_data.append(temp_list)
    with open('bitmap.csv', 'r') as csvfile:
        csv_reader = csv.reader(csvfile, delimiter=',')
        for row in csv_reader:
            temp_list = []
            for value in row:
                temp_list.append(value)
            bitmap_data.append(temp_list)
    with open('inode.csv', 'r') as csvfile:
        csv_reader = csv.reader(csvfile, delimiter=',')
        for row in csv_reader:
            temp_list = []
            for value in row:
                temp_list.append(value)
            inode_data.append(temp_list)
    with open('directory.csv', 'r') as csvfile:
        csv_reader = csv.reader(csvfile, delimiter=',')
        for row in csv_reader:
            temp_list = []
            for value in row:
                temp_list.append(value)
            directory_data.append(temp_list)
    with open('indirect.csv', 'r') as csvfile:
        csv_reader = csv.reader(csvfile, delimiter=',')
        for row in csv_reader:
            temp_list = []
            for value in row:
                temp_list.append(value)
            indirect_data.append(temp_list)

    #unallocated block (in inode.csv, group.csv, and bitmap.csv)
    free_blocks = []
    free_inodes = []
    used_block_bitmaps = []
    used_inode_bitmaps = []
    used_blocks = []
    for row in group_data:
        used_block_bitmaps.append(int(row[5],16))
        used_inode_bitmaps.append(int(row[4],16))
    for bitmap_row in bitmap_data:
        for block_row in used_block_bitmaps:
            if int(bitmap_row[0],16) == block_row:
                free_blocks.append(int(bitmap_row[1]))
            else:
                free_inodes.append(int(bitmap_row[1]))
    for row in inode_data:
        for x in range(11,26):
            if int(row[x],16) != 0:
                used_blocks.append(int(row[x],16))
    for x in used_blocks:
        for y in free_blocks:
            if x == y:
                read_inode_data(x)

    #duplicately allocated block
    seen = set()
    multiply_referenced_blocks = set()
    for x in used_blocks:
        if x not in seen:
            seen.add(x)
        else:
            multiply_referenced_blocks.add(x)
    for x in multiply_referenced_blocks:
        read_duplicate_inode_data(x)

    #unallocated inode
    inode_numbers = []
    directory_inode_numbers = []
    for x in inode_data:
        inode_numbers.append(int(x[0]))
    for x in directory_data:
        if int(x[4]) not in inode_numbers:
            f.write("UNALLOCATED INODE < " + int(x[4]) + " > REFERENCED BY DIRECTORY < " + int(x[0]) + " > ENTRY < " + int(x[1]) + " >\n")
        else:
            directory_inode_numbers.append(int(x[4]))

    #missing inodes
    zero_link_count = []
    missing_inodes = []
    for x in inode_data:
        if int(x[5]) == 0:
            zero_link_count.append(int(x[0]))
    for x in zero_link_count:
        if x not in free_inodes and x > 10:
            missing_inodes.append(x)
            inode_bitmap_number = used_inode_bitmaps[int(x/2048)]
            f.write("MISSING INODE < " + str(x) + " > SHOULD BE IN FREE LIST < " + str(inode_bitmap_number) + " >\n")

    #incorrect link count
    directory_inode_count = Counter(directory_inode_numbers)
    for x in inode_data:
        if int(x[0]) > 10 and int(x[0]) not in missing_inodes and int(x[5]) != directory_inode_count.get(int(x[0])):
            f.write("LINKCOUNT < " + x[0] + " > IS < " + x[5] + " > SHOULD BE < " + str(directory_inode_count.get(int(x[0]))) + " >\n")

    #incorrect directory entry
    root = int()
    for x in directory_data:
        if x[5] == ".":
            if int(x[0]) != int(x[4]):
                f.write("INCORRECT ENTRY IN < " + x[0] + " > NAME < " + x[5] + " > LINK TO < " + x[4] + " > SHOULD BE < " + x[0] + " >\n")
        if x[5] == "..":
            if int(x[0]) == int(x[4]):
                root = int(x[4])
            correct = 0
            correct_parent = str()
            for y in directory_data:
                if int(x[0]) == int(y[4]) and y[5] != "." and y[5] != "..":
                    correct_parent = y[0]
                    if int(x[4]) == int(y[0]):
                        correct = 1
                        break
            if correct == 0 and int(x[4]) != root:
                f.write("INCORRECT ENTRY IN < " + x[0] + " > NAME < " + x[5] + " > LINK TO < " + x[4] + " > SHOULD BE < " + correct_parent + " >\n")

    #invalid block pointer
    for x in inode_data:
        blocks_range = int(x[10])
        if int(x[10]) >= 13:
            blocks_range = 13
        for y in range(0,blocks_range):
            if int(x[11+y],16) <= 0:
                f.write("INVALID BLOCK < " + str(int(x[11+y],16)) + " > IN INODE < " + x[0] + " > ENTRY < " + str(y) + " >\n")

if __name__ == "__main__":
    main()
