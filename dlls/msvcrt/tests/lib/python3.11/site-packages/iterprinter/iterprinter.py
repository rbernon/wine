import re 
from collections import OrderedDict

class IterationPrinter:
	r"""

	Parameters
	----------
	kwargs:
		Dictionary of formatting for each line
	"""
	def __init__(self, **kwargs):
		self.columns = OrderedDict()
		# Copy over dictionary to 
		for key in kwargs:
			try:
				width = re.findall('\d+',kwargs[key])[0]
			except ValueError: 
				raise ValueError("Must provide a width in the format")

			self.columns[key] = {'width':int(width), 'fmt': kwargs[key]}

	def header(self, **kwargs):
		line1 = ""
		line2 = ""
		for k, key in enumerate(self.columns):
			width = self.columns[key]['width']
			if len(kwargs[key]) > width:
				# Update the width
				width = len(kwargs[key])
				self.columns[key]['fmt'] = self.columns[key]['fmt'].replace(f'{self.columns[key]["width"]}', f'{width}', 1)

			# Update width to be at least wide enough
			self.columns[key]['width'] = max(self.columns[key]['width'], len(kwargs[key]))
			line1 += f" {kwargs[key]:^{width}} \u2502"
			
			line2 += '\u2500'*(2+self.columns[key]['width']) 

			if k + 1 != len(self.columns):
				# If this is the last column, we have a special termination symbol like -|
				line2 += '\u253c'
			else:
				# normally this symbol looks like + 
				line2 += '\u2524'

		return line1, line2
	
	def print_header(self, **kwargs):
		line1, line2 = self.header(**kwargs)
		print(line1)
		print(line2)


	def iter(self, **kwargs):
		line = ''
		for key in self.columns:
			try:
				if kwargs[key] is None:
					raise KeyError
				line += f' {kwargs[key]:{self.columns[key]["fmt"]}} \u2502'
			except KeyError:
				# If the data isn't provided for the current line, leave it blank
				line += ' ' + ' '*self.columns[key]['width'] + ' \u2502'
		return line

	def print_iter(self, **kwargs):
		line = self.iter(**kwargs)
		print(line)


if __name__ == '__main__':
	printer = IterationPrinter(it = '4d', x='6f', dumb = '2', long = '20')
	printer.print_header(it = 'iter', x = 'long value', dumb = 'dumb', long = 'short')
	printer.print_iter(it = 2, x = 1.24234, dumb = 'str', long = 'asdf')
	
