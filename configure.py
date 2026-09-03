import sys
from ambuild2 import run

parser = run.BuildParser(sourcePath = sys.path[0], api='2.2')
parser.options.add_argument('--targets', type=str, dest='targets', default=None,
		                help="Override the target architecture (use commas to separate multiple targets).")
parser.options.add_argument('--enable-tests', default=False, dest='enable_tests', action='store_true',
                       help='Build tests.')
parser.Configure()