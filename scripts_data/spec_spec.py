import matplotlib.pyplot as plt
import sys
import os


def spec(tn, fp):
    return round(tn / (tn + fp), 3)


def sens(tp, fn):
    return round(tp / (tp + fn), 3)


def means(lst):
    return sum(lst) / len(lst)


dir_names = ["stats_0", "stats_0.02", "stats_0.04", "stats_0.06", "stats_0.08", "stats_0.1", "stats_0.12", "stats_0.14",
             "stats_0.16", "stats_0.18", "stats_0.2"]
home = os.path.expanduser("~")
plot_dir = " "
dir_to_res_path = home+ '/server_results/likeMaster/'
spec_dict = {key: 0 for key in dir_names}
sens_dict = {key: 0 for key in dir_names}

# over all stats dirs
for s_dir in dir_names:

    spec_list, sens_list = [], []
    # over all files in stats dirs
    stat_dir = os.path.join(dir_to_res_path, s_dir)
    print(stat_dir)
    for file in os.listdir(stat_dir):
        # open file - else quit programm
        if file.endswith(".txt"):
            with open(os.path.join(stat_dir, file), 'r') as stat_file:
                lines = stat_file.readlines()
                for line in lines:
                    nums = [int(i) for i in line.split() if i.isnumeric()]
                    # [tn, fp, tp ,fn, refrecsize]
                    spec_list.append(spec(nums[0], nums[1]))
                    sens_list.append(sens(nums[2], nums[3]))
                    sens_dict[s_dir] = means(sens_list)
                    spec_dict[s_dir] = means(spec_list)
        else:
            sys.exit("UNKWOWN FILE TYPE")



plt.plot([(x[6:]) for x in dir_names], list(spec_dict.values()), color=[0, 0.6, 1, 0.5],
         linestyle='-', marker='o', fillstyle='none', label='Specificity', markerfacecolor='white')
plt.plot([(x[6:]) for x in dir_names], list(sens_dict.values()), color=[1, 0.2, 0, 0.5],
         linestyle='-', marker='o', fillstyle='none', label='Sensitivity', markerfacecolor='white')
plt.legend(numpoints=1)
plt.ylim(0, 1.1)
plt.title('Sensitivity and Specificity of Stem Loops', pad=20, fontweight='bold')
plt.savefig(os.path.join(dir_to_res_path, 'result.png'))
plt.show()


plt.clf()
